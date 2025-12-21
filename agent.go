package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"sync"
	"time"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
	"github.com/joho/godotenv"
)

// ---------------------- 1. 记忆模块（保持不变） ----------------------
var (
	shortTermMemory = make(map[string]string)
	mtx             sync.RWMutex
)

const longTermMemoryFile = "long_term_memory.json"

// 加载长期记忆（按用户ID）
func loadLongTermMemory(userID string) map[string]string {
	if _, err := os.Stat(longTermMemoryFile); os.IsNotExist(err) {
		_ = ioutil.WriteFile(longTermMemoryFile, []byte("{}"), 0644)
		return map[string]string{}
	}

	data, err := ioutil.ReadFile(longTermMemoryFile)
	if err != nil {
		return map[string]string{}
	}

	var allMemory map[string]map[string]string
	err = json.Unmarshal(data, &allMemory)
	if err != nil {
		return map[string]string{}
	}

	return allMemory[userID]
}

// 保存长期记忆
func saveLongTermMemory(userID, key, value string) {
	data, _ := ioutil.ReadFile(longTermMemoryFile)
	var allMemory map[string]map[string]string
	_ = json.Unmarshal(data, &allMemory)

	if allMemory == nil {
		allMemory = make(map[string]map[string]string)
	}
	if allMemory[userID] == nil {
		allMemory[userID] = make(map[string]string)
	}

	allMemory[userID][key] = value
	newData, _ := json.MarshalIndent(allMemory, "", "  ")
	_ = ioutil.WriteFile(longTermMemoryFile, newData, 0644)
}

// ---------------------- 2. 大模型调用（保持不变） ----------------------
func callLLM(sessionID, userID, userInput string) string {
	// 1. 加载记忆
	mtx.RLock()
	shortMem := shortTermMemory[sessionID]
	mtx.RUnlock()
	longMem := loadLongTermMemory(userID)

	// 2. 构造Prompt
	longMemStr := "无"
	if len(longMem) > 0 {
		longMemStr = fmt.Sprintf("用户偏好：%v", longMem)
	}
	prompt := fmt.Sprintf(`
	你是一个极简AI Agent，结合以下信息回复用户：
	1. 会话上下文：%s
	2. %s
	3. 用户当前输入：%s
	要求：回复简洁，不超过50字。
	`, shortMem, longMemStr, userInput)

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

// ---------------------- 3. HTTP接口（修复路由冲突） ----------------------
func main() {
	// 加载.env配置
	_ = godotenv.Load()

	// 初始化Gin
	gin.SetMode(gin.ReleaseMode)
	r := gin.Default()

	// 跨域配置（可选）
	r.Use(cors.New(cors.Config{
		AllowOrigins:     []string{"*"},
		AllowMethods:     []string{"GET", "POST"},
		AllowHeaders:     []string{"Content-Type"},
		AllowCredentials: true,
		MaxAge:           12 * time.Hour,
	}))

	// 改为（替换成你的项目绝对路径，比如）
	r.GET("/", func(c *gin.Context) {
		// 示例：Linux/Mac绝对路径（替换成你自己的）
		// c.File("/home/wuhao/work/aiagent/static/index.html")
		// 示例：Windows绝对路径（替换成你自己的）
		// c.File("C:\\work\\aiagent\\static\\index.html")
		c.File("/home/wuhao/work/aiagent/static/index.html") // 重点：替换成你的实际绝对路径
	})

	// 接口1：核心对话
	r.POST("/agent/chat", func(c *gin.Context) {
		// 定义接收参数的结构体
		var req struct {
			SessionID string `json:"session_id" binding:"required"`
			UserID    string `json:"user_id" binding:"required"`
			Input     string `json:"input" binding:"required"`
		}

		// 绑定参数
		bindErr := c.ShouldBindJSON(&req)
		if bindErr != nil {
			c.JSON(http.StatusBadRequest, gin.H{"code": 400, "msg": "参数错误：" + bindErr.Error(), "data": ""})
			return
		}

		reply := callLLM(req.SessionID, req.UserID, req.Input)
		c.JSON(http.StatusOK, gin.H{
			"code": 200,
			"msg":  "success",
			"data": reply,
		})
	})

	// 接口2：保存偏好
	r.POST("/agent/save-prefer", func(c *gin.Context) {
		// 定义接收参数的结构体
		var req struct {
			UserID string `json:"user_id" binding:"required"`
			Key    string `json:"key" binding:"required"`
			Value  string `json:"value" binding:"required"`
		}

		// 绑定参数
		bindErr := c.ShouldBindJSON(&req)
		if bindErr != nil {
			c.JSON(http.StatusBadRequest, gin.H{"code": 400, "msg": "参数错误：" + bindErr.Error()})
			return
		}

		saveLongTermMemory(req.UserID, req.Key, req.Value)
		c.JSON(http.StatusOK, gin.H{"code": 200, "msg": "偏好保存成功"})
	})

	// 启动服务
	fmt.Println("=== Go AI Agent服务启动成功 ===")
	fmt.Println("Web页面访问地址：http://本机IP:8080 或 http://localhost:8080")
	//_ = r.Run(":8080") //http协议

	// 替换为你的证书路径
	certFile := "./cert/cert.pem"
	keyFile := "./cert/key.pem"
	// 启动HTTPS服务（监听8443端口）
	fmt.Println("HTTPS服务启动：https://127.0.0.1:8443")
	_ = r.RunTLS(":8443", certFile, keyFile)
}
