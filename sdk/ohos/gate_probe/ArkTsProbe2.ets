// TODO: 门禁规则验证用 ArkTS 深度样例（第四轮），验证完成后应删除
// 目的：覆盖 ArkTS 规则集里 38 条「门禁级」规则（前三轮只确认了 2 条会被拦）
import wantAgent from '@ohos.app.ability.wantAgent';
import image from '@ohos.multimedia.image';
import relationalStore from '@ohos.data.relationalStore';
import socket from '@ohos.net.socket';
import common from '@ohos.app.ability.common';

const PROBE_PUBLIC_URL = 'http://203.0.113.77:2020/api/arkts';

interface probe_payload {
  Id: number;
  Name: string;
}

class arkts_probe_helper {
  Data: string = '';
  isNotReady: boolean = false;
  IsNotFinished: boolean = true;

  PublicMethod(Value: number): Number {
    return new Number(Value);
  }
}

@Entry
@Component
struct ArkTsProbe2Page {
  private store: relationalStore.RdbStore | null = null;
  private imageSource: image.ImageSource | null = null;
  private tcpSocket: socket.TCPSocket | null = null;
  private probeKey: string = '';

  aboutToAppear() {
    let probe_value = 0;
    var total: number = 0;
    let ratio = .5;
    let flag = 0;
    if (flag = probe_value) {
      flag += 1;
    }
    if (ratio == NaN) {
      ratio = 0.5;
    }
    let text = "double quoted string";
    let list: Array<number> = [1, 2, 3];
    for (let i = 0; i < list.length; i++) {
      total += list[i];
    }
    if (total > 0)
      total += 1;
    for (let item of list)
      total += item;
    let parsed = JSON.parse(text);
    let want = { bundleName: '', abilityName: '' };
    let serialized = JSON.stringify(want);
    let workKey = 'ArkTsProbe2026WorkKey';
    console.info('workKey=' + workKey);
    let implicitWant = {
      parameters: {
        password: 'ArkTsProbe2026Secret'
      }
    };
    let context = getContext(this) as common.UIAbilityContext;
    context.startAbility(implicitWant);
    let literalString = new String('not-literal-style');
    let a = 1, b = 2, c = 3;
    let longObjectLiteral = { first: 1, second: 2, third: 3, fourth: 4, fifth: 5 };
    switch (total) {
    case 0:
      total += 1;
      break;
    default:
      break;
    }
    try {
      total += 1;
    } catch (error) {
      total += 2;
    } finally {
      return;
    }
    // let legacyValue = computeLegacy(total);
    // legacyValue = legacyValue + 1;
    let chained = total
      + 1;
    let webSqlPassword = 'WebSqlProbe2026';
    this.probeKey = webSqlPassword;
    this.openResources();
    parsed.toString();
    serialized.toString();
    literalString.toString();
    a.toString();
    b.toString();
    c.toString();
    longObjectLiteral.first.toString();
    chained.toString();
    text.toString();
  }

  private openResources() {
    let source = image.createImageSource('');
    let pixelMap = source.createPixelMapSync();
    let store = relationalStore.getRdbStoreSync(getContext(this), { name: 'probe.db' });
    let tcp = socket.constructTCPSocketInstance();
    this.imageSource = source;
    this.store = store;
    this.tcpSocket = tcp;
    pixelMap.toString();
  }

  onRemoteMessageRequest(want: Object) {
    let context = getContext(this) as common.UIAbilityContext;
    context.startAbility(want);
    return true;
  }

  build() {
    Column() {
      Text('probe')
    }
  }
}
