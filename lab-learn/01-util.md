# 用真实 grep 实现 xv6 简化版支持的四种模式（`^ . * $`）

真实 grep 本身就支持这些正则符号，语法和 xv6 简化版几乎一样。下面给出具体命令和例子。

## 前提：先有个测试文件

```bash
cat > test.txt <<'EOF'
hello world
foo bar
hallo there
heo
helo
say hello again
world end
EOF
```

## 1. `^` —— 匹配行首

找出以 `hello` 开头的行：

```bash
grep '^hello' test.txt
```

输出：

```text
hello world
```

- `^` 表示"行首"。
- 不加 `^` 就变成"行里任意位置出现 hello"。

## 2. `$` —— 匹配行尾

找出以 `world` 结尾的行：

```bash
grep 'world$' test.txt
```

输出：

```text
hello world
```

- `$` 表示"行尾"。

## 3. `.` —— 匹配任意单个字符

找出匹配 `h.llo` 的行（h + 任意字符 + llo）：

```bash
grep 'h.llo' test.txt
```

输出：

```text
hello world
hallo there
```

- `.` 占一个位置，随便是什么字符。

## 4. `*` —— 前一个字符重复 0 次或多次

找出匹配 `hel*o` 的行（he + 若干 l + o）：

```bash
grep 'hel*o' test.txt
```

输出：

```text
heo
helo
hello world
```

- `l*` 表示 l 出现 0 次、1 次、多次都行。
- 注意 `hello world` 里的 `hello` 部分匹配了。

## 5. 组合使用

```bash
grep '^h.llo' test.txt       # 行首 + h + 任意字符 + llo
grep 'hello.*world' test.txt # hello 后面跟任意内容再跟 world（.* 表示任意多字符）
grep '^$' test.txt           # 空行（^ 和 $ 之间什么都没有）
```

## 和 xv6 代码的对应关系

| xv6 代码 | 真实 grep 命令 |
|----------|----------------|
| `grep '^hello' file` | `grep '^hello' file` |
| `grep 'world$' file` | `grep 'world$' file` |
| `grep 'h.llo' file` | `grep 'h.llo' file` |
| `grep 'hel*o' file` | `grep 'hel*o' file` |

语法几乎完全一样，因为这些正则规则就是 Unix 通用的。

## 几个实用注意点

### 1. 一定要加单引号

```bash
grep '^hello' test.txt    # 对
grep ^hello test.txt      # 可能被 shell 解释，尤其是 $ * 等符号
```

`$`、`*`、`.` 在 shell 里有特殊含义，用**单引号**包起来最安全。

### 2. 从标准输入读（对应 xv6 里 fd=0 的情况）

```bash
echo "hello world" | grep 'hello'
```

不给文件名时，grep 从管道/键盘读。

### 3. 忽略大小写

```bash
grep -i 'hello' test.txt   # 匹配 HELLO、Hello 等
```

### 4. 显示行号

```bash
grep -n 'hello' test.txt
```

### 5. 反向匹配（输出不含某模式的行）

```bash
grep -v 'hello' test.txt
```

## 小实验：验证 `.` 和 `*`

```bash
echo "hello"  | grep 'h.llo'   # 有输出 → hello 匹配 h.llo
echo "hllo"   | grep 'h.llo'   # 无输出 → 中间缺一个字符
echo "heo"    | grep 'hel*o'   # 有输出 → l 出现 0 次
echo "hellllo"| grep 'hel*o'   # 有输出 → l 出现多次
```

## 一句话总结

真实 grep 和 xv6 那个简化版**用的是同一套正则语法**，你直接：

```bash
grep '模式' 文件名
```

把 `^ . * $` 写进模式里就行，只是记得**用单引号包住模式**。