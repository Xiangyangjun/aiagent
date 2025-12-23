package main

import (
	"fmt"
	"net/http"
	"os"
	"strings"
	"time"

	"simple-go-agent/llm"
	"simple-go-agent/memory"
	"simple-go-agent/tts" // 新增：导入TTS包

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
	"github.com/joho/godotenv"
)

func main() {
	// 加载.env（可将TTS的API Key放到.env中）
	_ = godotenv.Load()
	// 可选：从.env读取API Key（推荐）
	if apiKey := os.Getenv("ALIYUN_TTS_KEY"); apiKey != "" {
		tts.AliyunAPIKey = apiKey
	}

	gin.SetMode(gin.DebugMode)
	r := gin.Default()

	// 跨域配置（不变）
	r.Use(cors.New(cors.Config{
		AllowOrigins:     []string{"*"},
		AllowMethods:     []string{"GET", "POST"},
		AllowHeaders:     []string{"Content-Type"},
		AllowCredentials: true,
		MaxAge:           12 * time.Hour,
	}))

	// 静态页面（不变）
	r.GET("/", func(c *gin.Context) {
		c.File("/home/wuhao/work/aiagent/static/index.html")
	})

	// 核心对话接口（集成TTS）
	// 核心对话接口（集成TTS）
	r.POST("/agent/chat", func(c *gin.Context) {
		var req struct {
			SessionID string `json:"session_id" binding:"required"`
			UserID    string `json:"user_id" binding:"required"`
			Input     string `json:"input" binding:"required"`
		}
		if bindErr := c.ShouldBindJSON(&req); bindErr != nil {
			c.JSON(http.StatusBadRequest, gin.H{
				"code": 400,
				"msg":  "参数错误：" + bindErr.Error(),
				"data": nil,
			})
			return
		}

		// 校验UserID
		if strings.TrimSpace(req.UserID) == "" {
			c.JSON(http.StatusBadRequest, gin.H{
				"code": 400,
				"msg":  "UserID不能为空",
				"data": nil,
			})
			return
		}

		// 1. 调用大模型生成文本回复
		replyText, err := llm.CallLLM(req.SessionID, req.UserID, req.Input)
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{
				"code": 500,
				"msg":  "生成回复失败：" + err.Error(),
				"data": nil,
			})
			return
		}

		// 2. 调用TTS生成语音（返回URL）
		var audioURL string
		var ttsErr error
		ttsChan := make(chan struct{})
		go func() {
			defer close(ttsChan)
			audioURL, ttsErr = tts.GenerateSpeech(replyText) // 现在返回URL
			if ttsErr != nil {
				fmt.Printf("[警告] 生成语音失败：%v\n", ttsErr)
			}
		}()

		// 3. 保存短期记忆
		memory.SaveShortTerm(memory.ChatRound{
			SessionID: req.SessionID,
			UserID:    req.UserID,
			Input:     req.Input,
			Reply:     replyText,
			Timestamp: time.Now(),
		})

		// 4. 等待TTS完成
		<-ttsChan

		// 5. 构造返回数据（文本+音频URL）
		responseData := gin.H{
			"text":      replyText,     // 文本回复
			"audio_url": audioURL,      // 音频临时URL（核心修改）
			"tts_ok":    ttsErr == nil, // 语音生成是否成功
			"tts_err":   "",            // 错误信息（可选）
		}
		if ttsErr != nil {
			responseData["tts_err"] = ttsErr.Error()
		}

		// 返回结果
		c.JSON(http.StatusOK, gin.H{
			"code": 200,
			"msg":  "success",
			"data": responseData,
		})
	})

	// 保留原有保存偏好接口（兼容）
	r.POST("/agent/save-prefer", func(c *gin.Context) {
		var req struct {
			UserID string `json:"user_id" binding:"required"`
			Key    string `json:"key" binding:"required"`
			Value  string `json:"value" binding:"required"`
		}
		if bindErr := c.ShouldBindJSON(&req); bindErr != nil {
			c.JSON(http.StatusBadRequest, gin.H{
				"code": 400,
				"msg":  "参数错误：" + bindErr.Error(),
			})
			return
		}

		if req.Key == "keywords" {
			memory.MergeAndSaveLongTerm(req.UserID, req.Value)
		}

		c.JSON(http.StatusOK, gin.H{
			"code": 200,
			"msg":  "偏好保存成功",
		})
	})

	// 启动服务
	fmt.Println("=== Go AI Agent服务启动成功 ===")
	fmt.Println("Web页面访问地址：http://localhost:8443")
	certFile := "./cert/cert.pem"
	keyFile := "./cert/key.pem"
	err := r.RunTLS(":8443", certFile, keyFile)
	if err != nil {
		fmt.Printf("=== 服务启动失败！错误原因：%v ===\n", err)
		fmt.Println("尝试以HTTP方式启动...")
		errHTTP := r.Run(":8443")
		if errHTTP != nil {
			fmt.Printf("HTTP启动也失败：%v\n", errHTTP)
			return
		}
	}
}
