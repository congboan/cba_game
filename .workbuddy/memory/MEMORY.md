# 项目长期约定

## Git 提交惯例
- Commit message 使用中文或中英双语，不用纯英文。

## UI 框架架构决策（2026-08-06）
- 方向：通用插件 SettingsFramework（MVVM 重写 GameSettings，功能 1:1 保留），不引入 GameSettings 插件
- 分层：DataAsset 定义 → USettingViewModelBase 树 → ListView 条目绑定（W_SettingsEntry_*）
- 值存储注入式（反射路径 BindingPath），插件不依赖项目类型
- Spec 2 待定：UI 骨架（CBAActivatableWidget 等）
