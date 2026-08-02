# zmk-iidx

ZMK本体を変更せず、通常のZMK USB keyboardとIIDX向けUSB HIDを併用する
Zephyr/ZMKモジュールです。USB上では常に次の2 interfaceとして列挙されます。

```text
HID_0 = ZMK standard keyboard/consumer HID
HID_1 = IIDX 5-byte joystick HID
```

起動時はkeyboard modeです。`&iidx_boot` を割り当てたキーを押したまま起動すると
IIDX modeへ切り替わります。起動判定時間の終了後は、同じキーが通常の
fallback behaviorとして動作します。

## IIDX HID report

Report IDなしの5バイトです。Report Descriptorは参照機器と同じ50バイト列です。

| byte | 内容 |
| --- | --- |
| 0 | X軸、`int8_t`、絶対値 `-127..127` |
| 1 | Y軸、`int8_t`、絶対値 `-127..127` |
| 2 bit 0..6 | 1～7鍵 |
| 2 bit 7 | 常に0 |
| 3 bit 0..3 | E1～E4 |
| 3 bit 4..7 | 常に0 |
| 4 | 常に `0x00` |

## ZMK configへの追加

ZMK config側の `config/west.yml` の `projects` にこのリポジトリを追加します。

```yaml
manifest:
  projects:
    - name: zmk-iidx
      url: https://github.com/YOUR-NAME/zmk-iidx
      revision: main
```

keyboard/shieldの `.conf` に次を追加します。

```conf
# HID_0: ZMK keyboard, HID_1: IIDX
CONFIG_ZMK_USB=y
CONFIG_ZMK_IIDX_HID=y
CONFIG_USB_HID_DEVICE_COUNT=2

CONFIG_USB_DEVICE_VID=0x1CCF
CONFIG_USB_DEVICE_PID=0x8048
CONFIG_USB_DEVICE_SN=""

CONFIG_USB_SELF_POWERED=n
CONFIG_USB_MAX_POWER=50
CONFIG_USB_HID_POLL_INTERVAL_MS=4

# 3番目のCDC ACM interfaceを追加しない
CONFIG_ZMK_USB_LOGGING=n
```

VID/PIDはcomposite USB device全体に適用されます。IIDX interfaceは2番目のHIDに
なるため、Configuration Descriptorとendpoint addressは1-interfaceの参照機器とは異なります。

## IIDX vendor BLE

BLEを使用する場合は `.conf` に次を追加します。

```conf
CONFIG_ZMK_BLE=y
CONFIG_ZMK_IIDX_BLE=y

CONFIG_BT_DEVICE_NAME="IIDX Entry model"
CONFIG_BT_DEVICE_NAME_MAX=16

# Preferred Peripheral Connection Parameters
# 6 * 1.25 ms = 7.5 ms
CONFIG_BT_PERIPHERAL_PREF_MIN_INT=6
CONFIG_BT_PERIPHERAL_PREF_MAX_INT=6
CONFIG_BT_PERIPHERAL_PREF_LATENCY=0
CONFIG_BT_PERIPHERAL_PREF_TIMEOUT=400
```

このモジュールは次のvendor-specific GATT serviceを追加します。ZMK標準の
HID over GATT serviceも併存します。

```text
Primary Service:       UUID 0xFF00
Notify Characteristic: UUID 0xFF01
```

IIDX mode中にclientがCCCでnotifyを有効化すると、約16.667msごとに5バイトを
2回送信します。合計は約60 frame/秒、約120 notify/秒です。

```text
byte 0: scratch = USB reportのX軸値
byte 1: 0x00
byte 2 bit 0..6: B1..B7
byte 3 bit 0..1: E1..E2
byte 4: sequence counter
```

1 frame目のsequenceは1と2、2 frame目は3と4となり、それぞれ2ずつ増加します。
E3/E4とY軸はBLE packetでは使用しません。

frame intervalと接続時に要求するconnection intervalは次で上書きできます。

```conf
CONFIG_ZMK_IIDX_BLE_FRAME_INTERVAL_US=16667
CONFIG_ZMK_IIDX_BLE_CONN_INTERVAL=6
CONFIG_ZMK_IIDX_BLE_CONN_TIMEOUT=400
```

connection intervalはperipheral側から要求しますが、最終的な値はAndroid等のcentral側が
決定します。

### USB優先

USB HIDが利用可能になると次の動作になります。

1. IIDX vendor BLE notifyを停止
2. ZMK標準keyboardのpreferred transportをUSBに変更
3. USBを抜くと、ZMKのfallbackでBLE keyboardへ戻る
4. IIDX modeかつCCC購読が残っていればvendor notifyも再開

BLE接続自体はUSB接続中も維持されますが、IIDX vendor packetは送信しません。

現時点でadvertising packetへ `0xFF00` は追加しません。clientは名前等で接続した後、
GATT Service Discoveryで `0xFF00` / `0xFF01` を検出する前提です。

## behaviorとkeymap

keymapのルートに、button、mode、boot-selectのbehavior nodeを定義します。

```dts
/ {
    behaviors {
        iidx_btn: iidx_button {
            compatible = "zmk,behavior-iidx-button";
            #binding-cells = <1>;
        };

        iidx_mode: iidx_mode {
            compatible = "zmk,behavior-iidx-mode";
            #binding-cells = <0>;
            iidx-layer = <1>;
            keyboard-layer = <0>;
        };

        iidx_boot: iidx_boot {
            compatible = "zmk,behavior-iidx-boot-select";
            #binding-cells = <0>;

            /* 1つ目: 起動中、2つ目: 起動後の通常動作 */
            bindings = <&iidx_mode>, <&kp F1>;
            boot-timeout-ms = <1000>;
        };
    };
};
```

起動時はkeyboard modeのため、keyboard layerをlayer 0にします。例では最初の
物理キーが起動判定キーで、起動後は `F1` として動作します。

```dts
/ {
    keymap {
        compatible = "zmk,keymap";

        keyboard_layer {
            bindings = <
                &iidx_boot
                &kp N2 &kp N3 &kp N4 &kp N5 &kp N6 &kp N7
                &kp A  &kp S  &kp D  &kp F
                &iidx_mode
            >;
        };

        iidx_layer {
            bindings = <
                &iidx_btn 0  &iidx_btn 1  &iidx_btn 2  &iidx_btn 3
                &iidx_btn 4  &iidx_btn 5  &iidx_btn 6
                &iidx_btn 8  &iidx_btn 9  &iidx_btn 10 &iidx_btn 11
                &iidx_mode
            >;
        };
    };
};
```

| binding | IIDX report上のボタン |
| --- | --- |
| `&iidx_btn 0` .. `6` | 1～7鍵 |
| `&iidx_btn 8` .. `11` | E1～E4 |

bit 7および12～15はreservedのため使用できません。

## 起動時のmode判定

`&iidx_boot` を割り当てたキーの押下時刻で判定します。

- `boot-timeout-ms` 以内の押下: 1つ目のbinding、`&iidx_mode` を実行
- 判定時間の終了後: 2つ目のbinding、例では `&kp F1` を実行

ZMKのmatrix scan開始時に押されているキーは起動直後の押下イベントに
なるため、押したまま起動するとIIDX modeが選択されます。判定は時間窓方式の
ため、起動後1秒以内に新たにキーを押した場合もIIDX modeになります。
ボードの起動時間に応じて `boot-timeout-ms` を調整してください。

## mode切替の動作

`&iidx_mode` の押下時に次を実行します。

1. HID_0のkeyboard/consumer reportを全解放
2. HID_1のX/Y/button reportをすべて0にして送信
3. 有効modeを切り替え
4. `iidx-layer` または `keyboard-layer` へ移動

これにより、切替時にキーやIIDX buttonが押しっぱなしになるのを防ぎます。

## X/Y API

encoderや独自input listenerからは `<zmk_iidx/hid.h>` をincludeして呼び出せます。

```c
zmk_iidx_hid_set_axis(ZMK_IIDX_AXIS_X, 32);
zmk_iidx_hid_adjust_axis(ZMK_IIDX_AXIS_Y, -1);
zmk_iidx_hid_set_axes(0, 0);
```

範囲外の値は `-127..127` へclampされます。keyboard mode中はIIDX reportを
送信せず、これらのAPIは `-EACCES` を返します。

## 注意

- Zephyrの旧USB device stack APIを使用します。
- `CONFIG_USB_HID_DEVICE_COUNT=2` が必須です。
- USB product/manufacturer文字列も参照機器と合わせる場合は、
  `CONFIG_USB_DEVICE_PRODUCT` / `CONFIG_USB_DEVICE_MANUFACTURER` を `.conf` で指定してください。
