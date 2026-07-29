# Git 学习：用 CLion 和命令行管理 BatchGuard

本文的目标不是背完 Git，而是理解 CLion 的 Git 界面背后发生了什么，并能在
没有 IDE 时独立完成检查、提交、同步、分支和安全撤销。

## 1. Git 管理的四个位置

```text
工作区
  │ git add
  ▼
暂存区
  │ git commit
  ▼
本地仓库
  │ git push
  ▼
GitHub 远程仓库
```

- 工作区：正在编辑的实际文件。
- 暂存区：明确选入下一次提交的内容。
- 本地仓库：`.git/` 中保存的提交历史。
- 远程仓库：GitHub 上的共享版本。

`git add` 不会上传，`git commit` 也不会上传；只有 `git push` 才会把本地提交
发送到 GitHub。

CLion 默认可以直接勾选文件并 Commit。这个界面把“选择内容、准备提交、创建
提交”合并成了一个操作，但底层仍然遵循相同的 Git 模型。

## 2. CLion 操作与命令对应关系

| CLion 操作 | 命令行作用 |
|---|---|
| Local Changes | `git status` |
| 查看文件 Diff | `git diff` |
| 勾选提交文件 | 选择要暂存和提交的内容 |
| Commit | `git add` + `git commit` |
| Commit and Push | `git commit` + `git push` |
| Push | `git push` |
| Update Project | 通常相当于 `git pull` |
| Git Log | `git log` |
| New Branch | `git switch -c <分支>` |
| Checkout Branch | `git switch <分支>` |
| Rollback | 通常相当于 `git restore` |

即使主要使用 CLion，也应在提交窗口中逐个查看 Diff，不要只根据文件名判断。

## 3. 一次性配置

检查 Git：

```powershell
git --version
```

设置提交身份：

```powershell
git config --global user.name "你的名字"
git config --global user.email "你的邮箱"
```

检查配置及来源：

```powershell
git config --global --list
git config --list --show-origin
```

以上身份已经配置时，不需要重复设置。

## 4. 创建仓库与连接 GitHub

在项目根目录初始化：

```powershell
git init
git branch -M main
```

检查是否位于仓库中：

```powershell
git rev-parse --show-toplevel
git status
```

添加远程仓库：

```powershell
git remote add origin https://github.com/jiweidada/BatchGuard.git
git remote -v
```

第一次推送并建立跟踪关系：

```powershell
git push -u origin main
```

`origin` 只是远程仓库的本地别名，`main` 是分支名。设置上游后，后续通常只需：

```powershell
git push
git pull
```

修改远程地址：

```powershell
git remote set-url origin <新的仓库地址>
```

删除远程配置：

```powershell
git remote remove origin
```

## 5. 使用 .gitignore

构建产物、IDE 私有配置和本机缓存不应提交。BatchGuard 根目录建议创建
`.gitignore`：

```gitignore
/build/
/cmake-build-*/
/.idea/
```

查看被忽略的文件：

```powershell
git status --ignored
git check-ignore -v build
git check-ignore -v cmake-build-debug
```

`.gitignore` 只影响尚未被跟踪的文件。如果某个文件已经提交，需要停止跟踪但
保留本地文件：

```powershell
git rm --cached <文件>
git rm -r --cached <目录>
```

然后提交这次变更。不要为了让 `.gitignore` 生效而直接删除重要文件。

## 6. 查看当前状态

完整状态：

```powershell
git status
```

简短状态：

```powershell
git status --short
git status --short --branch
```

常见标记：

```text
?? file.cpp   未跟踪
 M file.cpp   工作区已修改，尚未暂存
M  file.cpp   修改已暂存
A  file.cpp   新文件已暂存
D  file.cpp   删除已暂存
```

短格式有两列：左列表示暂存区，右列表示工作区。

## 7. 查看具体修改

查看尚未暂存的修改：

```powershell
git diff
git diff -- <文件>
```

查看已暂存、即将提交的修改：

```powershell
git diff --cached
git diff --staged
git diff --cached --stat
```

只查看变化文件名：

```powershell
git diff --name-only
git diff --cached --name-only
```

比较两个提交：

```powershell
git diff <旧提交> <新提交>
git diff <旧提交>..<新提交> -- <文件>
```

未跟踪文件不会显示完整 `git diff`，要先通过 `git status` 发现，再用编辑器检查。

## 8. 将修改放入暂存区

暂存单个文件：

```powershell
git add CMakeLists.txt
```

暂存多个文件或目录：

```powershell
git add apps include src
git add README.md docs/cmake学习.md
```

暂存当前目录下所有未被忽略的变化：

```powershell
git add .
```

只有在 `.gitignore` 正确并检查过 `git status` 后，才建议使用 `git add .`。

交互式选择文件中的部分修改：

```powershell
git add -p
```

取消暂存但保留工作区修改：

```powershell
git restore --staged <文件>
```

取消暂存全部内容：

```powershell
git restore --staged .
```

## 9. 创建提交

提交已暂存内容：

```powershell
git commit -m "feat: 添加命令行输入验证"
```

打开编辑器填写较长提交说明：

```powershell
git commit
```

查看刚创建的提交：

```powershell
git show HEAD
git show --stat HEAD
```

修改最近一次提交信息或补入遗漏内容：

```powershell
git add <遗漏文件>
git commit --amend
```

如果最近提交已经推送到共享远程，不要随意 `--amend` 后强制推送；优先创建新的
修正提交。

### 提交信息建议

一个提交只表达一个目的。BatchGuard 可使用：

```text
feat: 新增功能
fix: 修复缺陷
docs: 修改文档
test: 添加或修改测试
refactor: 重构但不改变行为
build: 修改构建系统
chore: 工程维护
```

示例：

```text
feat: 添加目录输入验证
fix: 修复中文路径显示
docs: 完善 CMake 学习文档
build: 拆分核心库与 CLI 目标
```

## 10. 查看提交历史

简洁历史：

```powershell
git log --oneline
git log --oneline --decorate --graph --all
```

查看最近五次提交：

```powershell
git log -5 --oneline
```

查看某个文件的历史：

```powershell
git log --oneline -- CMakeLists.txt
git log -p -- CMakeLists.txt
```

查看某次提交：

```powershell
git show <提交哈希>
git show --stat <提交哈希>
git show <提交哈希>:CMakeLists.txt
```

查看某行最后由哪个提交修改：

```powershell
git blame CMakeLists.txt
```

提交哈希通常可以使用前几位，例如 `be4bb89`。

## 11. 与远程仓库同步

查看远程信息：

```powershell
git remote -v
git remote show origin
```

只下载远程信息，不修改当前工作区：

```powershell
git fetch origin
```

查看本地与远程差异：

```powershell
git status --branch
git log HEAD..origin/main --oneline
git log origin/main..HEAD --oneline
git diff HEAD..origin/main
```

拉取远程提交，并只允许快进：

```powershell
git pull --ff-only
```

推送当前分支：

```powershell
git push
```

第一次推送新分支：

```powershell
git push -u origin <分支名>
```

推荐在开始工作前先 `fetch` 或 `pull --ff-only`，提交后再 `push`。

## 12. 分支工作流

查看分支：

```powershell
git branch
git branch -a
git branch -vv
```

创建并切换功能分支：

```powershell
git switch -c feature/cli-input
```

切换已有分支：

```powershell
git switch main
git switch feature/cli-input
```

从最新主分支开始新功能：

```powershell
git switch main
git pull --ff-only
git switch -c feature/file-discovery
```

把功能分支合并到主分支：

```powershell
git switch main
git merge feature/file-discovery
```

删除已经合并的本地分支：

```powershell
git branch -d feature/file-discovery
```

删除远程分支：

```powershell
git push origin --delete feature/file-discovery
```

`git branch -D` 会强制删除未合并分支，可能丢失提交，不要随意使用。

## 13. 临时保存未完成修改

需要切换分支，但修改还不适合提交时：

```powershell
git stash push -m "暂存 CLI 输入验证"
```

同时保存未跟踪文件：

```powershell
git stash push -u -m "暂存未完成工作"
```

查看暂存列表：

```powershell
git stash list
git stash show -p "stash@{0}"
```

恢复并删除最近一次 stash：

```powershell
git stash pop
```

恢复但保留 stash：

```powershell
git stash apply "stash@{0}"
```

删除指定 stash：

```powershell
git stash drop "stash@{0}"
```

stash 是临时工具，不应代替正常提交。

## 14. 文件移动与删除

让 Git 记录文件重命名或移动：

```powershell
git mv main.cpp apps/cli/main.cpp
```

删除文件并暂存删除：

```powershell
git rm <文件>
```

停止跟踪但保留本地文件：

```powershell
git rm --cached <文件>
```

Git 实际根据内容相似度识别重命名，`git mv` 是“移动文件并暂存变化”的便捷写法。

## 15. 安全撤销

### 取消暂存

保留代码，只从暂存区移除：

```powershell
git restore --staged <文件>
```

### 丢弃尚未暂存的修改

```powershell
git restore <文件>
```

这会覆盖工作区内容。执行前先使用：

```powershell
git diff -- <文件>
```

### 恢复某个文件到指定提交

```powershell
git restore --source=<提交哈希> -- <文件>
```

### 安全撤销已经提交或推送的版本

```powershell
git revert <提交哈希>
git push
```

`git revert` 不会删除历史，而是创建一个内容相反的新提交，适合已经发布的提交。

### 检查未跟踪文件清理范围

```powershell
git clean -n
git clean -nd
```

`-n` 只预览。`git clean -fd` 会真正删除未跟踪文件和目录，通常不可恢复，必须
确认预览结果后才能考虑执行。

### 关于 reset

```powershell
git reset --soft <提交>
git reset --mixed <提交>
git reset --hard <提交>
```

- `--soft`：移动分支，保留暂存区和工作区。
- `--mixed`：移动分支并取消暂存，保留工作区。
- `--hard`：同时覆盖暂存区和工作区。

初学阶段不要使用 `git reset --hard`，尤其不要对已经推送的共享分支重写历史。

## 16. 处理合并冲突

发生冲突后先查看：

```powershell
git status
```

冲突文件中会出现：

```text
  <<<<<<< HEAD
当前分支内容
  =======
另一分支内容
  >>>>>>> feature/example
```

上面为避免文档被 Git 误判而增加了两个前导空格；真实冲突文件中的三个标记位于
行首，没有这两个空格。

手动保留正确内容并删除标记，然后：

```powershell
git add <已解决文件>
git commit
```

放弃本次合并：

```powershell
git merge --abort
```

CLion 会提供三栏冲突编辑器，但解决后仍应检查最终 Diff。

## 17. 标签

为稳定版本创建标签：

```powershell
git tag -a v0.1.0 -m "BatchGuard MVP v0.1.0"
git show v0.1.0
git push origin v0.1.0
```

查看标签：

```powershell
git tag
git tag --list "v0.*"
```

阶段 1 骨架通常不急于打版本标签，MVP 可运行并通过测试后再评估。

## 18. BatchGuard 推荐日常流程

### 开始一个阶段

```powershell
git switch main
git pull --ff-only
git switch -c feature/stage-2-cli
```

### 开发过程中

```powershell
git status
git diff
cmake --build build/debug
```

### 提交前

```powershell
git status --short
git diff
git add <本次相关文件>
git diff --cached --stat
git diff --cached
```

### 提交与推送

```powershell
git commit -m "feat: 添加 CLI 参数解析"
git log -3 --oneline
git push -u origin feature/stage-2-cli
```

随后可在 GitHub 创建 Pull Request，审查后合并到 `main`。

## 19. CLion 提交检查清单

使用 CLion 提交确实更方便，但每次仍应确认：

1. Local Changes 中只勾选本次相关文件。
2. 逐个查看 Diff。
3. 不包含 `build/`、`cmake-build-*`、`.idea/`、`.exe` 或 `.lib`。
4. Debug/Release 构建或相关测试已经通过。
5. 提交信息说明“做了什么”，不是“改了一些东西”。
6. Commit 后在 Git Log 检查结果。
7. Push 后确认本地分支与远程分支同步。

CLion 负责减少操作步骤，Git 知识负责让你知道这些步骤是否安全。

## 20. 常用命令速查

```powershell
# 状态与差异
git status --short --branch
git diff
git diff --cached

# 暂存与提交
git add <文件>
git restore --staged <文件>
git commit -m "类型: 说明"

# 历史
git log --oneline --decorate --graph --all
git show <提交>

# 远程
git fetch origin
git pull --ff-only
git push

# 分支
git switch -c <新分支>
git switch <分支>
git branch -vv

# 安全撤销
git restore <文件>
git revert <提交>
```

## 21. 学习完成标准

你能够独立完成以下事情，就掌握了项目开发所需的 Git：

- 解释工作区、暂存区、本地仓库和远程仓库。
- 提交前检查 Diff，并只选择相关文件。
- 创建小而清晰的提交并推送。
- 从最新 `main` 创建功能分支。
- 区分 `restore --staged`、`restore` 和 `revert`。
- 解决简单冲突，或安全放弃合并。
- 确保构建产物和 IDE 私有文件不会进入仓库。
