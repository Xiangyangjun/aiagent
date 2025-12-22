// util/memory.go - 长期记忆管理工具，限制每个用户最多20条关键信息
package util

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"time"
)

// 常量定义
const (
	longTermMemoryFile = "long_term_memory.json" // 记忆存储文件路径
	maxMemoryItems     = 20                      // 每个用户最大记忆条目数
)

// MemoryItem 单个记忆项（带创建时间，用于排序和删除最早条目）
type MemoryItem struct {
	Key        string    `json:"key"`         // 记忆键（如keywords）
	Value      string    `json:"value"`       // 记忆值（如"无糖饮品，茶饮"）
	CreateTime time.Time `json:"create_time"` // 创建时间，用于判断最早条目
}

// UserMemory 单个用户的有序记忆结构（替代原无序map）
type UserMemory struct {
	Items []MemoryItem `json:"items"` // 按插入顺序存储记忆项
}

// OldAllMemory 旧版记忆结构（兼容历史数据）
type OldAllMemory map[string]map[string]string

// LoadLongTermMemory 加载指定用户的长期记忆（返回map，保持原有接口兼容）
// 返回值：用户的所有记忆键值对（最多20条）
func LoadLongTermMemory(userID string) map[string]string {
	// 1. 检查文件是否存在，不存在则创建空文件
	if _, err := os.Stat(longTermMemoryFile); os.IsNotExist(err) {
		_ = ioutil.WriteFile(longTermMemoryFile, []byte("{}"), 0644)
		return map[string]string{}
	}

	// 2. 读取文件内容
	data, err := ioutil.ReadFile(longTermMemoryFile)
	if err != nil {
		fmt.Printf("读取记忆文件失败：%v\n", err)
		return map[string]string{}
	}

	// 3. 优先解析为新结构（带时间的有序结构）
	var newAllMemory map[string]UserMemory
	err = json.Unmarshal(data, &newAllMemory)
	if err != nil {
		// 解析失败，兼容旧结构（无时间的map）
		var oldAllMemory OldAllMemory
		err2 := json.Unmarshal(data, &oldAllMemory)
		if err2 != nil {
			fmt.Printf("解析记忆文件失败（新旧结构均不兼容）：%v\n", err2)
			return map[string]string{}
		}

		// 将旧结构转换为新结构并保存（一次性兼容）
		newAllMemory = make(map[string]UserMemory)
		for uid, oldItems := range oldAllMemory {
			userMem := UserMemory{}
			for k, v := range oldItems {
				// 旧数据默认用当前时间填充（保证排序逻辑）
				userMem.Items = append(userMem.Items, MemoryItem{
					Key:        k,
					Value:      v,
					CreateTime: time.Now(),
				})
			}
			// 旧数据超过20条时，截断到20条（保留任意20条）
			if len(userMem.Items) > maxMemoryItems {
				userMem.Items = userMem.Items[:maxMemoryItems]
			}
			newAllMemory[uid] = userMem
		}

		// 保存转换后的新结构到文件
		newData, _ := json.MarshalIndent(newAllMemory, "", "  ")
		_ = ioutil.WriteFile(longTermMemoryFile, newData, 0644)
	}

	// 4. 提取当前用户的记忆，转换为map返回（保持原有接口不变）
	userMem, ok := newAllMemory[userID]
	if !ok {
		return map[string]string{}
	}
	result := make(map[string]string, len(userMem.Items))
	for _, item := range userMem.Items {
		result[item.Key] = item.Value
	}
	return result
}

// SaveLongTermMemory 保存用户的长期记忆（自动限制最多20条，超过则删除最早条目）
// 参数：userID-用户ID，key-记忆键，value-记忆值
func SaveLongTermMemory(userID, key, value string) {
	// 1. 读取现有记忆数据
	data, _ := ioutil.ReadFile(longTermMemoryFile)
	var allMemory map[string]UserMemory
	err := json.Unmarshal(data, &allMemory)
	if err != nil || allMemory == nil {
		allMemory = make(map[string]UserMemory) // 初始化空结构
	}

	// 2. 获取当前用户的记忆（无则初始化）
	userMem, ok := allMemory[userID]
	if !ok {
		userMem = UserMemory{Items: []MemoryItem{}}
	}

	// 3. 检查是否已存在该key，存在则更新值和时间（覆盖逻辑）
	updated := false
	for i, item := range userMem.Items {
		if item.Key == key {
			userMem.Items[i].Value = value
			userMem.Items[i].CreateTime = time.Now() // 更新时间为当前（视为最新条目）
			updated = true
			break
		}
	}

	// 4. 不存在则新增，同时检查数量限制
	if !updated {
		// 超过20条，删除最早的条目（CreateTime最小的）
		if len(userMem.Items) >= maxMemoryItems {
			oldestIdx := 0
			for i, item := range userMem.Items {
				if item.CreateTime.Before(userMem.Items[oldestIdx].CreateTime) {
					oldestIdx = i
				}
			}
			// 删除最早的条目（切片截断）
			userMem.Items = append(userMem.Items[:oldestIdx], userMem.Items[oldestIdx+1:]...)
			fmt.Printf("用户[%s]记忆超过%d条，已删除最早条目\n", userID, maxMemoryItems)
		}
		// 添加新条目（时间为当前）
		userMem.Items = append(userMem.Items, MemoryItem{
			Key:        key,
			Value:      value,
			CreateTime: time.Now(),
		})
	}

	// 5. 保存更新后的记忆到文件
	allMemory[userID] = userMem
	newData, err := json.MarshalIndent(allMemory, "", "  ")
	if err != nil {
		fmt.Printf("保存记忆文件失败：%v\n", err)
		return
	}
	_ = ioutil.WriteFile(longTermMemoryFile, newData, 0644)
}
