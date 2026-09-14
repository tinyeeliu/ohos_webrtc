// TODO: 门禁规则验证用 ArkTS 样例，验证完成后应删除
import webview from '@ohos.web.webview';
import privacyManager from '@ohos.privacyManager';
import cryptoFramework from '@ohos.security.cryptoFramework';
import preferences from '@ohos.data.preferences';

@Entry
@Component
struct GateProbePage {
  private controller: webview.WebviewController = new webview.WebviewController();
  private probeData: ESObject = null;
  @State message: any = '';

  aboutToAppear() {
    var count = 0;
    if (count == 0) {
      count = 1;
    }
    let keyGenerator = cryptoFramework.createAsyKeyGenerator('RSA1024');
    let parsed = JSON.parse(this.message);
    preferences.getPreferences(getContext(), 'probe').then((store) => {
      store.put('password', 'ArkTsProbe2026Secret');
    });
    privacyManager.startUsingPermission(100, 'ohos.permission.READ_AUDIO');
    this.probeData = parsed;
    keyGenerator.toString();
  }

  build() {
    Column() {
      Web({ src: 'http://203.0.113.77:6060/probe', controller: this.controller })
        .javaScriptAccess(true)
        .javaScriptProxy({ object: this.probeData, name: 'probe', methodList: ['run'], controller: this.controller })
        .domStorageAccess(true)
        .fileAccess(true)
      Button('probe')
        .onClick(() => {
          console.log('probe clicked');
          eval('this.message = "x"');
          const self = this;
          delete self.probeData;
          let items: Array<string> = ['a'];
          for (let i = 0; i < items.length; i++) {
            console.info(items[i]);
          }
        })
    }
  }
}
