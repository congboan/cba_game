# 项目长期约定

## Git 提交惯例
- Commit message 使用中文或中英双语，不用纯英文。

## UI 框架架构决策（2026-08-06）
- 方向：通用插件 SettingsFramework（MVVM 重写 GameSettings，功能 1:1 保留），不引入 GameSettings 插件
- 分层：DataAsset 定义 → USettingViewModelBase 树 → ListView 条目绑定（W_SettingsEntry_*）
- 值存储注入式（反射路径 BindingPath），插件不依赖项目类型
- Spec 2 待定：UI 骨架（CBAActivatableWidget 等）

## AAF 技术方向（2026-08-27 审阅）
- Fragment 全虚函数镜像架构否决（违反 ISP、语义冲突无裁决）；转具名插槽组合（Type Object+Strategy）
- Row 即运行时唯一真相源；不做编辑器 bake；预载缓存放授予期（runtime bake at grant time）
- OnSpawn 补 Lyra 四重守卫；NetSecurityPolicy 默认 ClientOrServer
- 暂缓项：激活组、失败消息总线、EventRouter queue-flush 化

## AAF 动态创建（2026-08-27 定案）
- 目的：JSON 等格式动态创建 GA；GA 类只承载机制；绑 CDO 的机制=冲突项
- Fragments 单数组(TInstancedStruct<FAssemblyFragmentBase>)，成本/冷却皆片；ExecutionStrategy 单槽不并入
- 引擎事实：四冷却函数漏斗于 GetCooldownGameplayEffect()；覆写 getter 即收敛，无双结算
- HandleGameplayEvent 双通道(触发表父链上溯+委托广播)；缺口=Row 无法声明触发 → B 方案必做(RespondToTags+Router 反查直激去重)
- N5 AssetTags 同批必做；N7 选双端对称 Give 契约(Spec.SourceObject 不复制)
- 队列：X-1 更名迁移 / X-2' 冷却片 getter 收敛(顺带灭 A1 平行容器) / B1 / B2；待拍板：B3 ToJson、A2 尾奏上浮
- 平台缺陷：Agent 工具无专用 adapter(subagent 派发被拒)；build_editor 成功态偶发误抛 exit6
