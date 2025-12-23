package tts

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net/http"
	"time"
)

// TTSConfig TTS配置（可从.env加载）
var (
	AliyunAPIKey = "sk-21c5679fdf204dc9928a322e2738a75f" // 你的API Key
	APIURL       = "https://dashscope.aliyuncs.com/api/v1/services/aigc/multimodal-generation/generation"
	Model        = "qwen3-tts-flash"
	Voice        = "Cherry"         // 音色
	LanguageType = "Chinese"        // 语言类型
	Timeout      = 10 * time.Second // 请求超时
)

// TTSRequest TTS接口请求结构体（不变）
type TTSRequest struct {
	Model  string `json:"model"`
	Input  Input  `json:"input"`
	Output Output `json:"output,omitempty"`
}

type Input struct {
	Text         string `json:"text"`
	Voice        string `json:"voice"`
	LanguageType string `json:"language_type"`
}

type Output struct {
	Format string `json:"format"` // 音频格式：wav/mp3
	Type   string `json:"type"`   // 输出类型：audio
}

// ---------------------- 关键修改：适配新的返回格式 ----------------------
// TTSResponse 新的TTS接口响应结构体
type TTSResponse struct {
	Output    TTSOutput `json:"output"`
	Usage     TTSUsage  `json:"usage"`
	RequestID string    `json:"request_id"`
}

type TTSOutput struct {
	Audio        TTSAudio `json:"audio"`
	FinishReason string   `json:"finish_reason"`
}

type TTSAudio struct {
	Data      string `json:"data"`       // 空字符串（无需关注）
	ExpiresAt int64  `json:"expires_at"` // URL过期时间（秒级时间戳）
	ID        string `json:"id"`         // 音频ID
	URL       string `json:"url"`        // 音频临时访问URL（核心）
}

type TTSUsage struct {
	Characters int `json:"characters"` // 消耗的字符数
}

// GenerateSpeech 生成语音，返回音频临时URL（替代原Base64）
func GenerateSpeech(text string) (string, error) {
	// 空文本直接返回空
	if text == "" {
		return "", fmt.Errorf("文本内容为空")
	}

	// 构建请求体（不变）
	reqBody := TTSRequest{
		Model: Model,
		Input: Input{
			Text:         text,
			Voice:        Voice,
			LanguageType: LanguageType,
		},
		Output: Output{
			Format: "wav", // 匹配返回格式（wav）
			Type:   "audio",
		},
	}

	// 序列化请求体
	jsonBody, err := json.Marshal(reqBody)
	if err != nil {
		return "", fmt.Errorf("序列化请求体失败：%w", err)
	}

	// 创建HTTP客户端（带超时）
	client := &http.Client{Timeout: Timeout}
	req, err := http.NewRequest("POST", APIURL, bytes.NewBuffer(jsonBody))
	if err != nil {
		return "", fmt.Errorf("创建请求失败：%w", err)
	}

	// 设置请求头
	req.Header.Set("Authorization", "Bearer "+AliyunAPIKey)
	req.Header.Set("Content-Type", "application/json")

	// 发送请求
	resp, err := client.Do(req)
	if err != nil {
		return "", fmt.Errorf("调用TTS接口失败：%w", err)
	}
	defer resp.Body.Close()

	// 读取响应体
	body, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("读取响应失败：%w", err)
	}

	// 检查响应状态码
	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("TTS接口返回错误：%s，响应内容：%s", resp.Status, string(body))
	}

	// 解析新格式的响应
	var ttsResp TTSResponse
	if err := json.Unmarshal(body, &ttsResp); err != nil {
		return "", fmt.Errorf("解析TTS响应失败：%w，响应内容：%s", err, string(body))
	}

	// 校验音频URL是否有效
	if ttsResp.Output.Audio.URL == "" {
		return "", fmt.Errorf("TTS接口未返回音频URL，响应：%s", string(body))
	}

	// 返回音频临时URL
	return ttsResp.Output.Audio.URL, nil
}
