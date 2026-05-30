# 贡献指南

感谢您对 Elara 项目的关注！我们欢迎任何形式的贡献。

## 如何贡献

### 报告 Bug

1. 使用 [Bug 报告模板](https://github.com/yourusername/elara/issues/new?template=bug_report.md) 创建 issue
2. 描述问题的复现步骤
3. 提供环境信息（开发板、ESP-IDF 版本等）
4. 如果可能，提供日志输出

### 提交功能请求

1. 使用 [功能请求模板](https://github.com/yourusername/elara/issues/new?template=feature_request.md) 创建 issue
2. 描述功能的使用场景
3. 说明期望的实现方式

### 提交代码

1. Fork 项目
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 创建 Pull Request

## 开发环境

### 前置条件

- ESP-IDF v6.0.1 或更高版本
- Python 3.8 或更高版本
- Git

### 设置开发环境

1. 克隆项目
```bash
git clone https://github.com/yourusername/elara.git
cd elara
```

2. 设置 ESP-IDF 环境
```bash
. $HOME/esp/esp-idf/export.sh
```

3. 设置目标芯片
```bash
make set-target
```

4. 编译项目
```bash
make build
```

## 代码规范

### 代码风格

- 使用 clang-format 格式化代码
- 遵循 [Google C++ 风格指南](https://google.github.io/styleguide/cppguide.html)
- 使用 4 空格缩进
- 行宽限制 120 字符

### 提交规范

使用 [Conventional Commits](https://www.conventionalcommits.org/) 规范：

```
<type>(<scope>): <subject>

<body>

<footer>
```

类型：
- `feat`: 新功能
- `fix`: Bug 修复
- `docs`: 文档更新
- `style`: 代码风格（不影响代码运行的变更）
- `refactor`: 重构
- `perf`: 性能优化
- `test`: 测试相关
- `chore`: 构建过程或辅助工具的变更

示例：
```
feat(ui): 添加表情动画支持

- 添加表情动画控件
- 支持多种表情切换
- 优化动画性能

Closes #123
```

### 文档规范

- 使用中文编写文档
- 为所有公共 API 添加注释
- 更新相关文档（README、CHANGELOG 等）

## 测试

### 运行测试

```bash
make test
```

### 编写测试

- 为新功能编写测试
- 确保所有测试通过
- 测试文件放在 `test/` 目录

## Pull Request 流程

1. 确保代码通过所有测试
2. 更新相关文档
3. 更新 CHANGELOG.md
4. 创建 Pull Request
5. 等待代码审查
6. 根据反馈进行修改
7. 合并到主分支

## 行为准则

### 我们的承诺

为了营造一个开放和友好的环境，我们作为贡献者和维护者承诺：

- 尊重所有参与者
- 接受建设性的批评
- 专注于对社区最有利的事情
- 对他人表示同情

### 不可接受的行为

- 使用性暗示的语言或图像
- 恶意评论或人身攻击
- 公开或私下的骚扰
- 未经许可发布他人的私人信息

## 联系方式

如有问题，请通过以下方式联系我们：

- 创建 issue
- 发送邮件到 [your-email@example.com]

## 许可证

通过贡献，您同意您的贡献将在 [MIT 许可证](LICENSE) 下获得许可。
