package memory

import (
	"fmt"
	"sync"
	"time"
)

// ChatRound 单轮对话结构体（记录核心信息）
type ChatRound struct {
	SessionID string    // 会话ID（用于区分不同会话）
	UserID    string    // 用户ID（核心标识）
	Input     string    // 用户输入内容
	Timestamp time.Time // 时间戳（用于排序和截断）
	Reply     string
}

// 全局存储+锁（保证并发安全）
var (
	shortTermStore = make(map[string][]ChatRound) // key=UserID，value=用户10轮对话列表
	shortLock      sync.RWMutex
	maxShortRounds = 10 // 短期记忆固定保留10轮
)

// SaveShortTerm 保存单轮对话，自动截断超过10轮的旧数据
func SaveShortTerm(round ChatRound) {
	shortLock.Lock()
	defer shortLock.Unlock()

	// 获取用户已有对话
	rounds := shortTermStore[round.UserID]
	// 追加新轮次
	rounds = append(rounds, round)
	// 截断：只保留最后10轮（最新的10轮）
	if len(rounds) > maxShortRounds {
		rounds = rounds[len(rounds)-maxShortRounds:]
	}
	// 写回存储
	shortTermStore[round.UserID] = rounds
}

// GetShortTermContext 获取用户近10轮对话上下文（拼接为大模型可识别的字符串）
func GetShortTermContext(userID string) string {
	shortLock.RLock()
	defer shortLock.RUnlock()

	rounds := shortTermStore[userID]
	if len(rounds) == 0 {
		return "无历史对话"
	}

	// 拼接上下文（按时间正序，标注轮次，方便大模型提取）
	var context string
	for i, round := range rounds {
		context += fmt.Sprintf("第%d轮用户输入：%s；", i+1, round.Input)
	}
	return context
}
