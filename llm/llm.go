package llm

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"simple-go-agent/memory"
	"strings"
)

// ---------------------- 仅保留：关键词提取+大模型调用逻辑（依赖memory包） ----------------------
// ExtractHabitKeywords 提取关键词（改为调用memory包的短期记忆）
func ExtractHabitKeywords(userID string) string {
	// 从memory包获取用户近10轮对话上下文（核心修正）
	shortContext := memory.GetShortTermContext(userID)

	// 构造提取关键词的Prompt（逻辑不变）
	prompt := fmt.Sprintf(`
请基于用户最近10轮对话上下文，提取其中明确提及的「习惯/爱好」类核心关键词，要求：
1. 仅返回中文关键词，用逗号分隔，无任何解释、说明或多余文字；
2. 关键词简洁（如：钓鱼、看电影、户外、跑步），不重复；
3. 只提取用户明确提及的内容，不猜测、不编造、不扩展；
4. 无相关习惯/爱好则返回"无"。

用户近10轮对话上下文：%s
`, shortContext)

	// 调用阿里通义千问（逻辑不变）
	apiKey := os.Getenv("DASHSCOPE_API_KEY")
	if apiKey == "" {
		return "无"
	}

	apiURL := "https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation"
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
			"temperature":   0.1,
			"result_format": "message",
			"max_tokens":    100,
		},
	}

	reqBodyBytes, err := json.Marshal(reqBody)
	if err != nil {
		return "无"
	}

	req, err := http.NewRequest("POST", apiURL, bytes.NewBuffer(reqBodyBytes))
	if err != nil {
		return "无"
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+apiKey)

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return "无"
	}
	defer resp.Body.Close()

	respBody, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return "无"
	}

	var respData map[string]interface{}
	err = json.Unmarshal(respBody, &respData)
	if err != nil {
		return "无"
	}

	// 提取关键词（逻辑不变）
	output, ok := respData["output"].(map[string]interface{})
	if !ok {
		return "无"
	}
	choices, ok := output["choices"].([]interface{})
	if !ok || len(choices) == 0 {
		return "无"
	}
	choice := choices[0].(map[string]interface{})
	message := choice["message"].(map[string]interface{})
	keywords := strings.TrimSpace(message["content"].(string))

	if keywords == "" || keywords == "无" {
		return "无"
	}
	return keywords
}

// CallLLM 调用大模型生成回复（核心修正：依赖memory包管理记忆）
func CallLLM(sessionID, userID, userInput string) (string, error) { // 修正：返回error，方便上层处理
	// 1. 读取长期记忆（改为调用memory包）
	longKeywords := memory.GetLongTerm(userID)
	longMemStr := fmt.Sprintf("用户偏好关键词：%s", longKeywords)
	if longKeywords == "无" {
		longMemStr = "用户暂无偏好信息"
	}

	// 2. 获取短期记忆上下文（调用memory包）
	shortMem := memory.GetShortTermContext(userID)

	// 3. 构造Prompt（逻辑不变）
	prompt := fmt.Sprintf(`
你是一个生活化、有同理心的AI助手，核心目标是基于用户的全量对话信息和长期偏好，生成有温度、个性化的回复。
【参考信息】
1. 历史会话上下文（最近10轮，按时间从旧到新排序）：%s
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

	// 4. 调用阿里通义千问（逻辑不变，修正错误处理）
	apiKey := os.Getenv("DASHSCOPE_API_KEY")
	if apiKey == "" {
		return "", fmt.Errorf("请先配置DASHSCOPE_API_KEY环境变量")
	}

	apiURL := "https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation"
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
		return "", fmt.Errorf("请求构造失败：%w", err)
	}

	req, err := http.NewRequest("POST", apiURL, bytes.NewBuffer(reqBodyBytes))
	if err != nil {
		return "", fmt.Errorf("请求创建失败：%w", err)
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Authorization", "Bearer "+apiKey)

	client := &http.Client{}
	resp, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("调用大模型失败：%w", err)
	}
	defer resp.Body.Close()

	respBody, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("解析响应失败：%w", err)
	}

	var respData map[string]interface{}
	err = json.Unmarshal(respBody, &respData)
	if err != nil {
		return "", fmt.Errorf("解析JSON失败：%w，响应内容：%s", err, string(respBody))
	}

	// 提取回复内容（修正错误处理）
	output, ok := respData["output"].(map[string]interface{})
	if !ok {
		return "", fmt.Errorf("响应格式错误，output字段不存在：%s", string(respBody))
	}
	choices, ok := output["choices"].([]interface{})
	if !ok || len(choices) == 0 {
		return "", fmt.Errorf("无回复内容，choices字段为空：%s", string(respBody))
	}
	choice := choices[0].(map[string]interface{})
	message, ok := choice["message"].(map[string]interface{})
	if !ok {
		return "", fmt.Errorf("响应格式错误，message字段不存在：%s", string(respBody))
	}
	reply, ok := message["content"].(string)
	if !ok {
		return "", fmt.Errorf("响应格式错误，content字段不存在：%s", string(respBody))
	}

	// 5. 提取关键词并更新长期记忆（调用memory包）
	newKeywords := ExtractHabitKeywords(userID)
	memory.MergeAndSaveLongTerm(userID, newKeywords)

	return reply, nil
}
