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
