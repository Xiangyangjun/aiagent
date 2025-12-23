package main

import (
	"fmt"
	"net/http"
	"simple-go-agent/llm"
	"simple-go-agent/memory"
	"strings"
	"time"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
	"github.com/joho/godotenv"
)

func main() {
	fmt.Println("=== Go AI Agent开始服务启动 ===")
	_ = godotenv.Load()

	gin.SetMode(gin.ReleaseMode)
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

	// 核心对话接口（修正逻辑）
	// agent.go 中/agent/chat接口（关键行强化）
	r.POST("/agent/chat", func(c *gin.Context) {
		var req struct {
			SessionID string `json:"session_id" binding:"required"`
			UserID    string `json:"user_id" binding:"required"` // 必须传UserID
			Input     string `json:"input" binding:"required"`
		}
		if bindErr := c.ShouldBindJSON(&req); bindErr != nil {
			c.JSON(http.StatusBadRequest, gin.H{
				"code": 400,
				"msg":  "参数错误：" + bindErr.Error(),
				"data": "",
			})
			return
		}

		// 强制校验UserID非空（接口层兜底）
		if strings.TrimSpace(req.UserID) == "" {
			c.JSON(http.StatusBadRequest, gin.H{
				"code": 400,
				"msg":  "UserID不能为空",
				"data": "",
			})
			return
		}

		// 1. 保存短期记忆（绑定UserID）
		memory.SaveShortTerm(memory.ChatRound{
			SessionID: req.SessionID,
			UserID:    req.UserID, // 严格传递原始UserID
			Input:     req.Input,
			Timestamp: time.Now(),
		})

		// 2. 调用大模型（传递UserID）
		reply, err := llm.CallLLM(req.SessionID, req.UserID, req.Input)
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{
				"code": 500,
				"msg":  "生成回复失败：" + err.Error(),
				"data": "",
			})
			return
		}

		c.JSON(http.StatusOK, gin.H{
			"code": 200,
			"msg":  "success",
			"data": reply,
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

	// 启动服务（不变）
	fmt.Println("=== Go AI Agent服务启动成功 ===")
	fmt.Println("Web页面访问地址：http://localhost:8443")
	certFile := "./cert/cert.pem"
	keyFile := "./cert/key.pem"
	err := r.RunTLS(":8443", certFile, keyFile)
	if err != nil {
		// 打印详细错误，这是定位问题的关键
		fmt.Printf("=== 服务启动失败！错误原因：%v ===\n", err)
		// 可选：自动降级为HTTP启动（避免卡壳）
		fmt.Println("尝试以HTTP方式启动...")
		errHTTP := r.Run(":8443")
		if errHTTP != nil {
			fmt.Printf("HTTP启动也失败：%v\n", errHTTP)
			return // 启动失败，退出程序
		}
	}
}
