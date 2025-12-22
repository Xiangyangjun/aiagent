// util/util.go
package util

import (
	"fmt"
	"strings"
	"sync"
	"time"
	"unicode"

	"github.com/yanyiwu/gojieba"
)

// 1. 定义包级别的单例变量和锁
var (
	jiebaInstance *gojieba.Jieba
	once          sync.Once // 保证只初始化一次
)

// 合并关键字的最大数量限制
const maxKeywordCount = 20

// 2. 初始化jieba分词器（单例模式，线程安全+失败兜底）
func getJieba() *gojieba.Jieba {
	once.Do(func() {
		jiebaInstance = gojieba.NewJieba()
		if jiebaInstance == nil {
			fmt.Printf("[ERROR] %s jieba分词器初始化失败！\n", getCurrentTime())
		}
	})
	return jiebaInstance
}

// 3. 释放jieba资源（供main函数调用）
func FreeJieba() {
	if jiebaInstance != nil {
		jiebaInstance.Free()
		fmt.Printf("[INFO] %s jieba分词器资源已释放\n", getCurrentTime())
	}
}

// 辅助函数：获取当前时间（用于日志标识）
func getCurrentTime() string {
	return time.Now().Format("2006-01-02 15:04:05")
}

// 扩展停用词库（过滤无关内容）
var stopWords = map[string]bool{
	// 基础停用词
	"的": true, "了": true, "是": true, "我": true, "你": true, "他": true,
	"她": true, "它": true, "在": true, "有": true, "和": true, "就": true,
	"都": true, "而": true, "及": true, "与": true, "也": true, "还": true,
	"吗": true, "呢": true, "吧": true, "啊": true, "哦": true, "嗯": true,
	"这": true, "那": true, "哪": true, "什么": true, "怎么": true, "为什么": true,
	"推荐": true, "一款": true, "适合": true, "喜欢": true, "是的": true,
	// 临时查询类停用词（核心过滤）
	"今天": true, "明天": true, "昨天": true, "天气": true, "怎么样": true, "如何": true,
	"多少": true, "何时": true, "何地": true, "哪里": true, "哪些": true, "查询": true,
	"问": true, "打听": true, "了解": true, "知道": true, "你好": true, "我喜欢什么": true,
}

// 偏好词词性（包含n/vn/nr/nz/nl/v，覆盖名词、动名词、动词）
var preferencePOS = map[string]bool{
	"n":  true, // 名词（熊猫、鱼、钓鱼）
	"vn": true, // 动名词（看电影）
	"nr": true, // 人名（婷婷）
	"nz": true, // 专名
	"nl": true, // 名词性惯用语
	"v":  true, // 动词（喜欢、钓鱼、看）
}

// 精准提取用户偏好关键字（核心修正：按斜杠/拆分分词字符串）
func ExtractKeywords(userInput string) string {
	// 获取jieba单例
	jieba := getJieba()
	if jieba == nil {
		fmt.Printf("[ERROR] %s jieba分词器未初始化，无法提取关键字\n", getCurrentTime())
		return "无"
	}

	// 预处理：彻底清理无效字符
	userInput = strings.TrimSpace(userInput)
	userInput = strings.Map(func(r rune) rune {
		if unicode.IsControl(r) {
			return -1
		}
		return r
	}, userInput)
	userInput = strings.ReplaceAll(userInput, "　", " ")
	userInput = strings.ReplaceAll(userInput, " ", "")

	if userInput == "" {
		fmt.Printf("[INFO] %s 用户输入为空，无需提取关键字\n", getCurrentTime())
		return "无"
	}

	// jieba分词+词性标注（返回[]string，格式："词汇/词性"）
	wordTagStrs := jieba.Tag(userInput)
	if len(wordTagStrs) == 0 {
		fmt.Printf("[INFO] %s jieba分词结果为空，输入内容：%s\n", getCurrentTime(), userInput)
		return "无"
	}

	// 筛选有效关键字
	keywordSet := map[string]bool{}
	for idx, wordTagStr := range wordTagStrs {
		// ========== 核心修正：按斜杠/拆分，而非空格 ==========
		parts := strings.Split(wordTagStr, "/")
		// 处理拆分异常（比如无斜杠、多斜杠）
		if len(parts) < 2 {
			fmt.Printf("[DEBUG] %s 第%d个分词格式异常，跳过：%s\n", getCurrentTime(), idx, wordTagStr)
			continue
		}

		// 提取词汇和词性（取前两个元素，避免多斜杠问题）
		word := strings.TrimSpace(parts[0])
		pos := strings.TrimSpace(parts[1])

		// 兜底：空值直接过滤
		if word == "" || pos == "" {
			fmt.Printf("[DEBUG] %s 第%d个分词为空（word/pos），跳过：%s\n", getCurrentTime(), idx, wordTagStr)
			continue
		}

		// 过滤：停用词、单字
		if len(word) <= 1 || stopWords[word] {
			fmt.Printf("[DEBUG] %s 第%d个分词被过滤（单字/停用词）：word=%s, pos=%s\n", getCurrentTime(), idx, word, pos)
			continue
		}

		// 保留偏好词性的词汇
		if preferencePOS[pos] {
			keywordSet[word] = true
			fmt.Printf("[DEBUG] %s 第%d个分词保留为关键字：word=%s, pos=%s\n", getCurrentTime(), idx, word, pos)
		} else {
			fmt.Printf("[DEBUG] %s 第%d个分词被过滤（非偏好词性）：word=%s, pos=%s\n", getCurrentTime(), idx, word, pos)
		}
	}

	// 转换为逗号分隔字符串返回
	keywords := []string{}
	for word := range keywordSet {
		keywords = append(keywords, word)
	}
	if len(keywords) == 0 {
		fmt.Printf("[INFO] %s 无有效偏好关键字，输入内容：%s\n", getCurrentTime(), userInput)
		return "无"
	}
	return strings.Join(keywords, "，")
}

// MergeKeywords 合并新旧关键字：去重+过滤停用词+限制数量
func MergeKeywords(existingKeywords, newKeywords string) string {
	if existingKeywords == "" || existingKeywords == "无" {
		existingKeywords = ""
	}
	if newKeywords == "" || newKeywords == "无" {
		newKeywords = ""
	}

	allWords := []string{}
	if existingKeywords != "" {
		allWords = append(allWords, strings.Split(existingKeywords, "，")...)
	}
	if newKeywords != "" {
		allWords = append(allWords, strings.Split(newKeywords, "，")...)
	}

	// 去重+过滤停用词
	wordMap := make(map[string]bool)
	uniqueWords := []string{}
	for _, word := range allWords {
		word = strings.TrimSpace(word)
		if word == "" || word == "无" || stopWords[word] {
			continue
		}
		if !wordMap[word] {
			wordMap[word] = true
			uniqueWords = append(uniqueWords, word)
		}
	}

	// 限制数量（超过20个保留最新的）
	if len(uniqueWords) > maxKeywordCount {
		uniqueWords = uniqueWords[len(uniqueWords)-maxKeywordCount:]
		fmt.Printf("[DEBUG] %s 关键字数量超过%d，已截断为最新的%d个\n", getCurrentTime(), maxKeywordCount, maxKeywordCount)
	}

	// 兜底返回
	if len(uniqueWords) == 0 {
		return "无"
	}
	return strings.Join(uniqueWords, "，")
}
