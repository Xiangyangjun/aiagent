package llm

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"simple-go-agent/util"
	"strings"
	"sync"
)

var (
	shortTermMemory = make(map[string]string)
	mtx             sync.RWMutex
)

// ---------------------- 2. 大模型调用（保持不变） ----------------------
func CallLLM(sessionID, userID, userInput string) string {
	// 1. 加载记忆
	mtx.RLock()
	shortMem := shortTermMemory[sessionID]
	mtx.RUnlock()
	longMem := util.LoadLongTermMemory(userID)

	fmt.Printf("longMem: %v\n", longMem)
	// 2. 构造Prompt
	longMemStr := "无"
	// 替换原有longMemStr拼接逻辑
	if len(longMem) > 0 {
		// 拼接自然的键值对字符串
		var memItems []string
		for key, value := range longMem {
			// 过滤空值/无意义值
			if value == "" || value == "无" {
				continue
			}
			// 键名更人性化（比如keywords→关键字，hobby→爱好）
			friendlyKey := map[string]string{
				"keywords": "关键字",
				"hobby":    "爱好",
				"prefer":   "偏好",
			}[key]
			if friendlyKey == "" {
				friendlyKey = key // 未匹配的键保留原名称
			}
			memItems = append(memItems, fmt.Sprintf("%s：%s", friendlyKey, value))
		}
		// 处理无有效记忆的情况
		if len(memItems) == 0 {
			longMemStr = "用户暂无有效偏好信息"
		} else {
			longMemStr = fmt.Sprintf("用户偏好：%s", strings.Join(memItems, "；"))
		}
	} else {
		longMemStr = "用户暂无偏好信息"
	}
	prompt := fmt.Sprintf(`
你是一个生活化、有同理心的AI助手，核心目标是基于用户的全量对话信息和长期偏好，生成有温度、个性化的回复。
【参考信息】
1. 历史会话上下文（按时间从旧到新排序）：%s
   - 规则：优先参考近3轮对话内容，确保回复承接上下文，不偏离用户对话逻辑
2. 用户的长期偏好/记忆（核心标签+偏好程度）：%s
   - 规则：仅作为个性化补充，不强行关联，避免偏离当前提问核心
3. 用户当前的提问/输入（含语气倾向）：%s

【回复核心要求】
1. 语气风格：亲切自然，贴合用户当前输入的语气（用户轻松则活泼，用户提问则耐心，用户倾诉则共情）；
2. 内容要求：优先精准回应当前提问，再自然融入匹配的长期偏好（如用户喜欢钓鱼则可轻提相关）；
3. 表达规范：避免生硬机器感、套话和模板化回复，用词生活化；
4. 字数控制：整体回复控制在80-120字，逻辑清晰、语句通顺，无冗余信息；
5. 避坑点：不编造未提及的偏好，不忽视历史对话中的关键信息，不使用专业术语。
`, shortMem, longMemStr, userInput)

	fmt.Printf("prompt: %s\n", prompt)
	// 3. 构造通义千问HTTP请求
	apiKey := os.Getenv("DASHSCOPE_API_KEY")
	if apiKey == "" {
		return "请先配置DASHSCOPE_API_KEY"
	}

	apiURL := "https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation"

	// 请求体
	reqBody := map[string]interface{}{
		"model": "qwen-turbo",
		"input": map[string]interface{}{
			"messages": []map[string]interface{}{
				{
					"role":    "user",
					"content": prompt,
				},
			},
		},
		"parameters": map[string]interface{}{
			"temperature":   0.5,
			"result_format": "message",
		},
	}
	reqBodyBytes, err := json.Marshal(reqBody)
	if err != nil {
		return "请求构造失败：" + err.Error()
	}

	// 发送HTTP请求
	req, err := http.NewRequest("POST", apiURL, bytes.NewBuffer(reqBodyBytes))
	if err != nil {
		return "请求创建失败：" + err.Error()
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+apiKey)

	// 执行请求
	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return "调用大模型失败：" + err.Error()
	}
	defer resp.Body.Close()

	// 解析响应
	respBody, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return "解析响应失败：" + err.Error()
	}

	// 解析JSON响应
	var respData map[string]interface{}
	err = json.Unmarshal(respBody, &respData)
	if err != nil {
		return "解析JSON失败：" + string(respBody)
	}

	// 提取回复内容
	output, ok := respData["output"].(map[string]interface{})
	if !ok {
		return "响应格式错误：" + string(respBody)
	}
	choices, ok := output["choices"].([]interface{})
	if !ok || len(choices) == 0 {
		return "无回复内容：" + string(respBody)
	}
	choice := choices[0].(map[string]interface{})
	message := choice["message"].(map[string]interface{})
	reply := message["content"].(string)

	// 4. 更新短期记忆
	newShortMem := fmt.Sprintf("%s\n用户：%s\nAgent：%s", shortMem, userInput, reply)
	mtx.Lock()
	shortTermMemory[sessionID] = newShortMem
	mtx.Unlock()

	return reply
}
