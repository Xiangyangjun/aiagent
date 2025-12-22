package main

import (
	"fmt"
	"net/http"
	"time"

	"simple-go-agent/llm"
	"simple-go-agent/util"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
	"github.com/joho/godotenv"
)

func main() {
	defer util.FreeJieba()
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
	// ---------------------- 原有代码不变，新增关键字提取函数后，修改chat接口 ----------------------
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

		// 核心新增步骤1：提取用户输入的关键字
		keywords := util.ExtractKeywords(req.Input)
		fmt.Printf("用户[%s]输入的关键字：%s\n", req.UserID, keywords) // 可选：日志打印

		// 核心新增步骤2：调用大模型获取回复
		reply := llm.CallLLM(req.SessionID, req.UserID, req.Input)

		// 若已有关键字，合并后再保存（避免覆盖）
		// 1. 加载用户已保存的历史关键字
		existingMem := util.LoadLongTermMemory(req.UserID)
		existingKeywords := existingMem["keywords"]

		// 2. 一行调用MergeKeywords：自动处理空值、去重、限制最多20个关键字
		// 入参：历史关键字 + 本次新提取的关键字（keywords）
		mergedKeywords := util.MergeKeywords(existingKeywords, keywords)

		// 3. 直接保存合并后的结果（无需if-else，MergeKeywords已兜底）
		util.SaveLongTermMemory(req.UserID, "keywords", mergedKeywords)

		// 返回结果（原有逻辑不变）
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

		util.SaveLongTermMemory(req.UserID, req.Key, req.Value)
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
