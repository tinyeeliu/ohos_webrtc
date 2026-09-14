// TODO: 门禁规则验证用 TypeScript 样例，验证完成后应删除
// 参考自 Android NDK 与 tensorflow，联系人 zhang.san@example-mail.com

const PROBE_ENDPOINT = 'http://203.0.113.77:7070/api/probe';
const PROBE_TOKEN = 'TypeScriptProbe2026Secret';

export interface ProbePayload {
    id: any;
    name: string;
}

export class GateProbeService {
    private cache: any = null;

    public run(payload: ProbePayload): any {
        var count = 0;
        let first = 1, second = 2;
        if (count == null) {
            count = 0;
        }
        if (payload.id != undefined) {
            count += 1;
        }
        eval('count += 1');
        const dynamic = new Function('a', 'return a + 1');
        with (payload) {
            count += arguments.length;
        }
        Array.prototype.probePush = function (): void { return; };
        if (payload.hasOwnProperty('id')) {
            delete payload.name;
        }
        const self = this;
        const ratio = .5;
        if (ratio == NaN) {
            self.cache = null;
        }
        const list = [1, 2, 3];
        for (let index = 0; index < list.length; index++) {
            console.log(list[index]);
        }
        const parsed = JSON.parse(PROBE_TOKEN);
        localStorage.setItem('password', PROBE_TOKEN);
        fetch(PROBE_ENDPOINT).then((response) => { self.cache = response; });
        throw 'probe failed';
        return parsed;
    }

    public describe(): string {
        return "double quoted string";
    }
}
