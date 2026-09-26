# git 操作手册（Task4 专用）

这份文档给零基础起步的自己用：日常怎么做提交、怎么写提交信息、写错了怎么救、
以及怎么把代码推到 GitHub。**考核标准里「提交代码 git commit 记录（注意原子化
提交，以及提交规范）」就看这些。**

---

## 一、这台机器上已经配好的东西

配置文件在 `~/.gitconfig`（全局生效，所有仓库共用）：

| 配置 | 值 | 作用 |
| --- | --- | --- |
| `user.name` | 一瓶凉茶 | 提交记录里的作者名 |
| `user.email` | 398150078@qq.com | 提交记录里的作者邮箱 |
| `init.defaultBranch` | `main` | 新建仓库默认主分支名 |
| `core.autocrlf` | `input` | 防止 Windows 换行符混进来制造 diff 噪音 |
| `core.quotepath` | `false` | 中文文件名不再显示成 `\344\270\255` |
| `commit.template` | `~/.gitmessage` | 每次 `git commit` 自动带出提交信息模板 |
| `credential.helper` | `store` | 记住 GitHub 凭据（见第七节的风险说明） |

随时用这两条命令确认身份没配错（配置错了，提交记录就白做了）：

```bash
git config -l --show-origin      # 列出所有配置及来源文件
git var GIT_AUTHOR_IDENT         # 应输出：一瓶凉茶 <398150078@qq.com> ...
```

> **注意**：`398150078@qq.com` 需要先在 GitHub 账号的
> Settings → Emails 里添加并验证，提交才会在 GitHub 上归到你名下。
> 如果你更想隐藏真实邮箱，可以用 GitHub 提供的
> `<用户名>@users.noreply.github.com`，改 `user.email` 即可。

---

## 二、核心循环：四条命令

```bash
git status                  # 1. 看现在有什么改动（最常用，随时敲）
git add <文件>              # 2. 把要提交的改动放进暂存区
git commit                  # 3. 提交（会自动弹出模板，写清动机）
git log --oneline -10       # 4. 看最近的提交历史
```

`git diff`（看还没 `add` 的改动）和 `git diff --staged`（看已经 `add` 的改动）
是 review 自己代码的主要手段，提交前至少要扫一眼。

---

## 三、原子化提交：一个提交只做一件事

**判断标准**：如果你的提交信息里出现了「顺便」「同时」「另外」，说明该拆成两个提交。

好处很实际 —— 出问题时能精确回退，review 的人也能顺着提交历史看懂你的思路，
这正是 Task4 想问的「你的迭代过程是怎样的」。

拆分技巧：用 `git add -p` 交互式挑选要提交的代码块（hunk），而不是
`git add .` 一把梭。`git add .` 之前一定要先 `git status` 确认没有误加
`build/` 目录或临时文件。

一个反例和一个正例：

```bash
# ✗ 反例：一个提交混了三件事，回退时没法只退其中一件
git commit -m "改了相机类，加了枚举，顺便修了 CMake"

# ✓ 正例：每个提交都能独立说清一件事
#   feat: 实现设备枚举接口
#   build: 补充 MVS 库的 rpath 配置
```

---

## 四、提交信息规范

格式：**类型前缀英文 + 描述中文**（Conventional Commits）。

```
<类型>: <一句话说清改了什么>

<空一行，然后写：为什么改 + 怎么改的，以及为什么没选别的方案>
```

| 类型 | 用在哪 |
| --- | --- |
| `feat` | 新增功能 |
| `fix` | 修 bug（正文必须写清触发条件和根因） |
| `refactor` | 重构，对外行为不变 |
| `build` | 构建系统 / CMake / 依赖变化 |
| `docs` | 只改文档 |
| `test` | 只改测试 |
| `chore` | 杂项，如初始化仓库、改 `.gitignore` |
| `style` | 纯格式调整，不影响逻辑 |

第一行控制在 50 字符内，用祈使语气（写「实现」而不是「实现了」）。

**对 Task4 特别重要的一点**：那个要求你讲清「迭代过程、为什么抛弃了原本的设计」
的文档，最好的素材就是提交历史里的 `refactor:` 和 `fix:` 提交。所以踩坑、
返工都要如实提交，不要怕历史不好看 —— 一路 `feat:` 到终点反而显得没有思考过程。

每个提交正文回答两个问题，例如：

```
refactor: 取流由 SDK 回调改为自有线程轮询

MV_CC_RegisterImageCallBackEx 的回调运行在 SDK 自己的线程里，在里面
既不能阻塞（会拖慢 SDK 收流）也不能抛异常（异常穿过 C 边界是未定义
行为），而且回调与 Camera 析构存在竞争：对象销毁时回调可能仍在执行。
改为自有线程轮询 MV_CC_GetImageBuffer，把线程启停时序完全掌握在类
内部，析构时先 join 再销毁句柄。
```

---

## 五、看历史：证明自己的思路

```bash
git log --oneline --graph --decorate   # 紧凑的图形化历史
git show <提交号>                       # 看某个提交改了哪些内容
git log --stat                         # 每个提交动了哪些文件、增删多少行
git blame <文件>                        # 逐行看某一行是哪个提交引入的
```

`git blame` 在排查「这行到底为什么这么写」时特别好用。

---

## 六、写错了怎么救

先分清两种情况：**还没推到 GitHub**，随便改；**已经推上去了**，不要改写历史。

### 提交信息写错了 / 漏了一个文件

```bash
git commit --amend            # 修改最近一次提交（会打开模板重新编辑）
git add <漏掉的文件>          # 如果漏文件，先 add 再 amend
```

### 想撤销最近一次提交，但保留改动

```bash
git reset --soft HEAD~1       # 撤销提交，改动退回暂存区（推荐）
git reset HEAD~1              # 撤销提交，改动退回工作区（未暂存）
```

⚠️ 不要用 `git reset --hard`，它会永久丢弃你的代码改动。

### 想丢弃某个文件的未提交改动

```bash
git restore <文件>             # 丢弃工作区改动（不可恢复！）
git restore --staged <文件>    # 把文件从暂存区拿出来，改动保留
```

### 已经推送的提交写错了

不要 `amend` + 强推（会破坏别人的历史）。用反向提交：

```bash
git revert <提交号>            # 生成一个「抵消该改动」的新提交
```

### 改崩了想回到某个时刻

```bash
git reflog                    # 查看 HEAD 的移动记录，找回"丢失"的提交
```

`reflog` 是 git 的安全网，绝大多数误操作都能靠它救回来。

---

## 七、推送到 GitHub

### 7.1 从零建一个仓库：完整流程

以后每开一个新项目都是这两段式：**网页上建一个空的 → 本地推上去。**

#### 第一段：在网页上建空仓库

1. GitHub 右上角 `+` → New repository。
2. 填 Repository name（如 `RP_task4`），选 Public / Private。
3. **下面三个初始化选项一个都不要勾**：Add a README file、Add .gitignore、
   Choose a license。让仓库建成完全空白。
4. 点 Create repository。

第 3 步是新手最容易踩的坑。如果勾了 README，GitHub 会先在远程仓库放一个提交，
而你的本地仓库也有自己的提交，两者没有共同祖先，首次 push 会被直接拒绝：

```
! [rejected]        main -> main (fetch first)
error: failed to push some refs to '...'
hint: Updates were rejected because the remote contains work that you do not have locally.
```

补救办法是 `git pull --rebase origin main`，把远程那个提交挪到本地提交下面再推 ——
能救回来，但平白多出一次合并。**空仓库没有这个问题，所以一开始就别勾。**
不勾的代价只是仓库首页暂时空着，等我们把 README 推上去就有了。

#### 第二段：把本地仓库推上去

创建成功后 GitHub 会显示一个引导页，上面有三段代码，它们对应三种不同处境，
**别乱选**：

| 页面上的代码块 | 适用情况 | 该不该用 |
| --- | --- | --- |
| `echo "# xxx" >> README.md` 开头那段 | 本地还没有仓库，从零开始 | ✗ 会重复 `git init` 并覆盖你的 README |
| 「快速安装」 | 用 GitHub 模板建仓库 | ✗ 不适用 |
| 「…或从命令行中推送现有的仓库」 | 本地已有仓库和提交 | ✓ **就是这段** |

还要注意引导页默认停在 `SSH` 标签页，URL 形如
`git@github.com:用户名/仓库名.git`。**本机 `~/.ssh` 是空的，用 SSH 地址会报
`Permission denied (publickey)`。** 点旁边的 `HTTPS` 标签，拿
`https://github.com/用户名/仓库名.git` 再用。两种方式的区别只是认证手段不同，
HTTPS 用 token，SSH 用密钥。

在本地仓库目录里执行：

```bash
git remote add origin https://github.com/<用户名>/<仓库名>.git
git branch -M main
git push -u origin main
```

逐条解释，因为这几条以后每次都用：

- `git remote add origin <URL>` —— 给远程仓库起个名字并记住地址。`origin` 只是
  **默认约定的名字**，不是关键字，但大家都用它。`git remote -v` 随时查看记住了哪些地址。
- `git branch -M main` —— 把当前分支强制改名成 `main`。只在本地误建出 `master`
  分支时才需要；用 `git init -b main` 建出来的本来就是 main，这条是保险。
- `git push -u origin main` —— 把本地 `main` 推到 `origin` 的 `main`。
  `-u` 是 `--set-upstream` 的缩写，**只在第一次推送时加**：它把本地 main 和远程
  origin/main 关联起来。关联之后 `git push` / `git pull` 不用再带参数，
  `git status` 也会告诉你「本地领先远程几个提交」。

提示 `Username` 时填 GitHub 用户名，提示 `Password` 时**必须填 Personal Access
Token，不能填账号密码**（GitHub 早已取消密码认证）。

### 7.2 建 Personal Access Token

GitHub 右上角头像 → Settings → Developer settings → Personal access tokens →
Tokens (classic) → Generate new token (classic)：

- Note 填个能认出来的用途，比如 `task4-laptop`；
- Expiration 选 90 天（不要把过期设成 no expiration）；
- 权限勾 **`repo`**（公开仓库理论上 `public_repo` 就够，但如果以后把仓库改成私有，
  只有 `repo` 才推得动，直接勾 `repo` 省事）；
- 点 Generate token，**token 只在生成时显示一次，当场复制保存**。

推送时会不会弹出输入提示，取决于系统有没有图形凭据对话框：本机是纯命令行环境，
所以在终端里执行 `git push` 会**直接出现 `Username for 'https://github.com':` 提示行**，
依次输入 GitHub 用户名和刚才那个 token 即可。输错 token 会报
`Authentication failed`，重跑一次 `git push` 再输就行，不会留下垃圾提交。

### 之后日常推送

```bash
git push
```

### ⚠️ 凭据安全（必读）

本机没有 `libsecret` 钥匙串助手，所以用的是 `store`，它把 token
**明文**保存在 `~/.git-credentials` 里：

```bash
chmod 600 ~/.git-credentials   # 确保只有自己能读
```

几条铁律：

- token 等同密码，**绝不能写进仓库任何文件**，`.gitignore` 里已挡掉
  `.git-credentials` / `*.token` / `*.pat`；
- 录屏和截图时注意别把终端里的 token 录进去；
- 一旦怀疑泄露，立刻去 GitHub 上 Revoke 该 token 并重新生成。

以后如果装上 gnome-keyring，可以升级成系统钥匙串（不再落明文）：

```bash
# sudo apt install libsecret-1-0
git config --global credential.helper libsecret
rm -f ~/.git-credentials
```

### 平时要检查远程状态时

```bash
git remote -v                 # 看远程地址
git status                    # 会提示本地领先/落后远程几个提交
git fetch && git log --oneline origin/main -5
```

---

## 八、分支：小任务用 main，大改动开分支

Task4 这类单人项目，小步提交直接落在 `main` 上是可以接受的。
但如果是「把取流从回调改成线程」这种会大范围动结构的改动，建议开分支，
改完再合并 —— 这样 `main` 上始终留着一个能跑通的版本。

```bash
git switch -c refactor/grab-thread     # 新建并切换到分支
# ...改代码、提交...
git switch main                        # 切回主分支
git merge refactor/grab-thread         # 合并（fast-forward）
git branch -d refactor/grab-thread     # 删掉已合并的分支
```

不要在一个分支上同时改两个功能，否则合并冲突会很难受。

---

## 九、Task4 建议的提交序列

按这个顺序开发，每个箭头对应一个原子提交：

```
chore: 初始化仓库与忽略规则                       ← 已完成
build: 添加顶层 CMake 与 FindMVS 模块
build: 配置 clangd 与 VSCode 智能感知
feat: 定义相机类型枚举与错误码映射
feat: 实现设备枚举程序（无相机可验证）
feat: 添加 Camera 类接口与 Pimpl 骨架
feat: 基于 SDK 回调实现取流
refactor: 取流由 SDK 回调改为自有线程轮询        ← 迭代过程的关键证据
feat: 实现像素格式转换与帧封装
feat: 实现参数读写与软触发
feat: 添加示例程序与使用说明
docs: 补充设计文档与无相机验证清单
test:  <如有>
```

每完成一项就跑一遍第二节的自我介绍循环（`status` → `add` → `commit`），
不要攒一大堆改动一次性提交。

---

## 十、每次提交前的自查清单

- [ ] `git status` 里没有 `build/`、图片输出等不该提交的文件
- [ ] `git diff --staged` 扫过了，没有调试用的 `printf` / 注释掉的死代码
- [ ] 代码能编译通过（提交点应该是可构建的）
- [ ] 提交信息第一行是 `<类型>: <中文描述>`，没有「顺便」
- [ ] 正文写清了「为什么改」，而不是只描述「改了什么」
