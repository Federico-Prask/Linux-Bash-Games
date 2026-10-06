#!/usr/bin/env bash
# F2：AI 自动对局统计脚本（用户 2026-10-05“按优先级全部较完整完成”）
#
# 用途：固定种子批量跑 N 局全 AI 观战，输出各阵营胜率、平均日志长度（近似对局长度）、
#       新机制触发次数与“平局（回合数达到上限）”次数，用来对比 AI 改动前后的效果。
#
# 用法：
#   tools/ai_bench.sh                # 默认：身份场 40 局（4~8 人，普通/至尊各半）+ 斗地主 24 局
#   tools/ai_bench.sh 100            # 身份场 100 局
#   THKS_BIN=./thks tools/ai_bench.sh 20
#
# 依赖：先 `make -j8`（脚本不会自动编译，避免污染计时）。
set -u

BIN="${THKS_BIN:-./thks}"
N_ID="${1:-40}"
N_DDZ="${2:-24}"
START_SEED="${START_SEED:-5000}"
TIMEOUT="${TIMEOUT:-120}"
OUT="${OUT:-/tmp/ai_bench.$$.log}"

if [ ! -x "$BIN" ]; then
  echo "找不到可执行文件 $BIN（请先 make -j8）" >&2
  exit 1
fi

: > "$OUT"
timeouts=0

# ---------- 身份场 ----------
for ((i=0;i<N_ID;i++)); do
  seed=$((START_SEED + i))
  n=$((4 + seed % 5))          # 4~8 人
  f=$((1 + seed % 2))          # 1 普通场 / 2 至尊场
  printf "1\n%d\n%d\n1\n3\n" "$f" "$n" | THKS_AI_DELAY=0 THKS_SEED="$seed" timeout "$TIMEOUT" "$BIN" >> "$OUT" 2>&1
  rc=$?
  [ "$rc" -ne 0 ] && { echo "  [超时/异常] 身份场 seed=$seed n=$n field=$f rc=$rc"; timeouts=$((timeouts+1)); }
done

# ---------- 斗地主 ----------
for ((i=0;i<N_DDZ;i++)); do
  seed=$((START_SEED + 1000 + i))
  f=$((1 + seed % 2))
  printf "2\n%d\n3\n" "$f" | THKS_AI_DELAY=0 THKS_SEED="$seed" timeout "$TIMEOUT" "$BIN" >> "$OUT" 2>&1
  rc=$?
  [ "$rc" -ne 0 ] && { echo "  [超时/异常] 斗地主 seed=$seed field=$f rc=$rc"; timeouts=$((timeouts+1)); }
done

count() { grep -o "$1" "$OUT" | wc -l | tr -d ' '; }

games=$(count '【游戏结束】')
ddz=$(count '斗地主结算')
draws=$(count '回合数达到上限')

echo "==================== AI 对局统计 ===================="
echo "样本：身份场 $N_ID 局 + 斗地主 $N_DDZ 局（种子 $START_SEED 起，固定可复现）"
echo "完赛：$games 局（含斗地主结算 $ddz 局） | 超时/异常：$timeouts | 平局(回合上限)：$draws"
echo
echo "-- 胜负分布 --"
grep -o '胜利阵营: .*' "$OUT" | sort | uniq -c | sort -rn
echo
echo "-- 身份场新机制触发 --"
echo "择途·明置身份牌 : $(count '明置身份牌：【内奸】')"
echo "择途·侍奉明主   : $(count '选择「侍奉明主」')"
echo "择途·自立为主   : $(count '选择「自立为主」')"
echo "立储            : $(count '为储君（太子）')"
echo "储君继位        : $(count '（忠臣）继位')"
echo
echo "-- AI 行为抽样 --"
echo "AI 换将(D1)     : $(count '【换将·AI】')"
echo "主公技授予/剥夺 : $(count '【主公技】')"
echo "无懈可击使用    : $(count '使用【无懈可击】')"
echo "拼点            : $(count '拼点：')"
echo "改判(鬼才/鬼道) : $(count '将判定牌替换为')"
echo
echo "-- 对局长度（日志行数，越短说明收尾越干脆）--"
awk '/核心引擎初始化完成/{if(n>0)print n; n=0} {n++} END{if(n>0)print n}' "$OUT" \
  | sort -n | awk '{a[NR]=$1; s+=$1} END{if(NR==0){print "无数据"; exit} printf "局数=%d 最短=%d 中位=%d 最长=%d 平均=%.0f\n", NR, a[1], a[int((NR+1)/2)], a[NR], s/NR}'
echo "====================================================="
echo "完整日志：$OUT"
