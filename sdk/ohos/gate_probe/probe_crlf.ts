// 门禁规则验证用 CRLF + BOM 样例（第四轮），验证完成后应删除
// 参考自 Android NDK 与 tensorflow

export class ProbeCrlf {
    public run(value: string): number {
        var total = 0;
        if (total == 0) {
            total = 1;
        }
        eval('total += 1');
        const parsed = JSON.parse(value);
        return total + parsed;
    }
}
