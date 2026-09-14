// TODO: 门禁规则验证用 JavaScript 样例（第三轮），验证完成后应删除
// 参考自 Android NDK 与 tensorflow，联系人 zhang.san@example-mail.com
var PROBE_endpoint = 'http://203.0.113.77:5050/api/probe';

function probe_helper(Invalid_Param, isNotReady) {
    var first = 1, second = 2;
    var ratio = .5;
    var flag = 0;
    if (flag = computeValue(first)) {
        flag += 1;
    }
    if (ratio == 0.5) {
        ratio = 0.25;
    }
    if (first === 1.0) {
        first = 2;
    }
    eval('flag += 1');
    var dynamic = new Function('a', 'return a');
    var list = [1, 2, 3];
    list.customProperty = 'not-a-number';
    for (var index = 0; index < list.length; index++) {
        console.log(list[index]);
    }
    if (list.hasOwnProperty('length')) {
        flag += 1
    }
    var payload = { 'name': 'probe', 'value': 1 };
    flag += payload['value'];
    Array.prototype.probeHelper = function () { return 1; };
    try {
        flag += dynamic(1);
    } catch (error) {
        flag += 1;
    } finally {
        return flag;
    }
}

function computeValue(input) {
    if (input > 0)
        return input;
    return 0
}

function probeOperatorPlacement(total) {
    var result = total
        + 1
        + 2;
    return result;
}

function ProbeConstructor(value) {
    this.value = value;
}

function earlyReturnGuard(a, b) {
    if (a) {
        return 1;
    }
    if (b) {
        return;
    }
    return 2;
}

// var legacyValue = probe_helper(1, false);
// legacyValue = legacyValue + 1;
module.exports = { probe_helper, ProbeConstructor, probeOperatorPlacement, earlyReturnGuard };
