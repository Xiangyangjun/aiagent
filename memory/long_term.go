package memory

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"time"
)

// 全局存储+锁（并发安全）
var (
	longTermStore = make(map[string]string) // key=UserID，value=逗号分隔的关键词字符串
	longLock      sync.RWMutex
	maxLongKeys   = 50                             // 单用户最多保留50个关键词
	persistFile   = "./data/long_term_memory.json" // 持久化文件路径

	// 新增：异步写文件相关
	writeChan   = make(chan map[string]string, 100) // 写文件通道（缓冲100，避免阻塞）
	closeChan   = make(chan struct{})               // 关闭协程信号
	initialized = false                             // 延迟初始化标记
	ioTimeout   = 5 * time.Second                   // IO超时时间
)

// ---------------------- 优化：延迟初始化 + 异步写文件协程 ----------------------
// Init 手动初始化（替代init函数，避免启动阶段卡住）
func Init() error {
	longLock.Lock()
	defer longLock.Unlock()

	if initialized {
		return nil // 避免重复初始化
	}

	// 1. 创建数据目录（容错，不panic）
	dataDir := filepath.Dir(persistFile)
	if err := os.MkdirAll(dataDir, 0755); err != nil {
		return fmt.Errorf("创建目录失败：%w", err)
	}

	// 2. 加载历史数据（带超时+容错）
	if err := loadFromFileWithTimeout(); err != nil {
		return fmt.Errorf("加载数据失败：%w", err)
	}

	// 3. 启动异步写文件协程（全局唯一）
	go asyncWriteLoop()

	initialized = true
	fmt.Println("长期记忆模块初始化完成")
	return nil
}

// ---------------------- 新增：带超时的文件加载 ----------------------
func loadFromFileWithTimeout() error {
	// 用通道控制超时
	resultChan := make(chan error, 1)
	go func() {
		// 文件不存在，初始化空存储
		if _, err := os.Stat(persistFile); os.IsNotExist(err) {
			longTermStore = make(map[string]string)
			resultChan <- saveToFileWithTimeout() // 生成空文件
			return
		}

		// 读取文件内容（不加锁，因为外层已加锁）
		fileContent, err := os.ReadFile(persistFile)
		if err != nil {
			resultChan <- fmt.Errorf("读取文件失败：%w", err)
			return
		}

		// JSON解析到内存
		if err := json.Unmarshal(fileContent, &longTermStore); err != nil {
			resultChan <- fmt.Errorf("解析文件失败：%w", err)
			return
		}

		resultChan <- nil
	}()

	// 超时控制
	select {
	case err := <-resultChan:
		return err
	case <-time.After(ioTimeout):
		return errors.New("加载文件超时")
	}
}

// ---------------------- 新增：带超时的文件写入 ----------------------
func saveToFileWithTimeout() error {
	// 先序列化（内存操作，快）
	data, err := json.MarshalIndent(longTermStore, "", "  ")
	if err != nil {
		return fmt.Errorf("序列化失败：%w", err)
	}

	// 异步写文件+超时控制
	resultChan := make(chan error, 1)
	go func() {
		// 写入文件（覆盖写，权限0644）
		err := os.WriteFile(persistFile, data, 0644)
		if err != nil {
			resultChan <- fmt.Errorf("写入文件失败：%w", err)
			return
		}
		resultChan <- nil
	}()

	// 超时控制
	select {
	case err := <-resultChan:
		return err
	case <-time.After(ioTimeout):
		return errors.New("写入文件超时")
	}
}

// ---------------------- 新增：异步写文件循环（后台协程） ----------------------
func asyncWriteLoop() {
	ticker := time.NewTicker(1 * time.Second) // 批量写间隔（1秒）
	defer ticker.Stop()

	var pendingData map[string]string // 待写入的数据
	for {
		select {
		case <-closeChan:
			// 收到关闭信号，写入最后一批数据
			if pendingData != nil {
				_ = savePendingData(pendingData)
			}
			return
		case data := <-writeChan:
			// 接收待写入数据（覆盖，保留最新）
			pendingData = data
		case <-ticker.C:
			// 定时批量写入
			if pendingData != nil {
				if err := savePendingData(pendingData); err != nil {
					fmt.Printf("异步写文件失败：%v\n", err)
				} else {
					pendingData = nil // 清空待写入
				}
			}
		}
	}
}

// 保存待写入数据（带重试）
func savePendingData(data map[string]string) error {
	// 序列化
	bytes, err := json.MarshalIndent(data, "", "  ")
	if err != nil {
		return err
	}

	// 重试3次
	for i := 0; i < 3; i++ {
		err := os.WriteFile(persistFile, bytes, 0644)
		if err == nil {
			return nil
		}
		time.Sleep(100 * time.Millisecond) // 重试间隔
	}
	return fmt.Errorf("重试3次后仍写入失败")
}

// ---------------------- 优化：合并保存逻辑（异步写文件，减少锁持有时间） ----------------------
// MergeAndSaveLongTerm 合并新提取的关键词，去重后保存（最多50个）
func MergeAndSaveLongTerm(userID, newKeywords string) string {
	// 1. 仅操作内存时加锁（缩短锁持有时间）
	longLock.Lock()
	// 空值兜底
	if newKeywords == "" || newKeywords == "无" {
		existing := longTermStore[userID]
		longLock.Unlock() // 立即释放锁
		if existing == "" {
			return "无"
		}
		return existing
	}

	// 2. 拆分新旧关键词（兼容逗号/顿号，去空格）
	existingStr := longTermStore[userID]
	existingKeys := splitKeywords(existingStr)
	newKeys := splitKeywords(newKeywords)

	// 3. 去重合并（用map去重）
	mergedMap := make(map[string]struct{})
	for _, k := range existingKeys {
		mergedMap[k] = struct{}{}
	}
	for _, k := range newKeys {
		mergedMap[k] = struct{}{}
	}

	// 4. 转为切片，限制最多50个
	mergedSlice := make([]string, 0, len(mergedMap))
	for k := range mergedMap {
		mergedSlice = append(mergedSlice, k)
	}
	if len(mergedSlice) > maxLongKeys {
		mergedSlice = mergedSlice[:maxLongKeys] // 超出50个则截断
	}

	// 5. 拼接并更新内存
	mergedStr := strings.Join(mergedSlice, "，")
	longTermStore[userID] = mergedStr

	// 6. 复制内存数据（避免协程竞争），放入异步写通道
	dataCopy := make(map[string]string, len(longTermStore))
	for k, v := range longTermStore {
		dataCopy[k] = v
	}
	longLock.Unlock() // 释放锁（IO操作前释放）

	// 7. 异步写入（非阻塞，通道满则丢弃旧数据）
	select {
	case writeChan <- dataCopy:
	default:
		// 通道满，替换最新数据
		<-writeChan
		writeChan <- dataCopy
		fmt.Printf("写文件通道满，替换旧数据（用户：%s）\n", userID)
	}

	return mergedStr
}

// ---------------------- 原有逻辑优化：GetLongTerm（无修改，仅保留） ----------------------
func GetLongTerm(userID string) string {
	longLock.RLock()
	defer longLock.RUnlock()

	keys := longTermStore[userID]
	if keys == "" {
		return "无"
	}
	return keys
}

// ---------------------- 辅助函数：拆分关键词（原有逻辑不变） ----------------------
func splitKeywords(str string) []string {
	if str == "" || str == "无" {
		return []string{}
	}
	str = strings.ReplaceAll(str, "，", ",")
	str = strings.ReplaceAll(str, "、", ",")
	parts := strings.Split(str, ",")

	var res []string
	for _, p := range parts {
		trimmed := strings.TrimSpace(p)
		if trimmed != "" {
			res = append(res, trimmed)
		}
	}
	return res
}

// ---------------------- 新增：优雅关闭（可选） ----------------------
// Close 优雅关闭异步写协程
func Close() {
	close(closeChan)
}

// ---------------------- 兼容原有init逻辑（可选，保证启动不报错） ----------------------
func init() {
	// 原有init改为容错初始化，不panic
	if err := Init(); err != nil {
		fmt.Printf("长期记忆模块初始化警告：%v\n", err)
		// 初始化失败仍继续，内存使用空存储
		longTermStore = make(map[string]string)
		go asyncWriteLoop()
		initialized = true
	}
}
