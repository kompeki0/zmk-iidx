# zmk-iidx

ZMK本体を変更せず、IIDX向けの専用USB HIDインターフェースを追加する
Zephyr/ZMKモジュールです。旧Zephyr USB device stackの `HID_0` を単独で使います。

## HID report

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

ZMK側の `west.yml` の `projects` にこのリポジトリを追加し、`config/west.yml`
から見えるようにします。リポジトリURLとrevisionは利用環境に合わせて変更してください。

```yaml
manifest:
  projects:
    - name: zmk-iidx
      url: https://github.com/YOUR-NAME/zmk-iidx
      revision: main
```

shield/boardの `.conf` に次を追加します。`ZMK_IIDX_HID` がUSB stackとHID classを
有効化するため、`CONFIG_USB=y` 等の個別指定は不要です。

```conf
CONFIG_ZMK_USB=n
CONFIG_ZMK_IIDX_HID=y

CONFIG_USB_DEVICE_VID=0x1CCF
CONFIG_USB_DEVICE_PID=0x8048
CONFIG_USB_DEVICE_SN=""

CONFIG_USB_SELF_POWERED=n
CONFIG_USB_MAX_POWER=50
CONFIG_USB_HID_POLL_INTERVAL_MS=4

CONFIG_ZMK_USB_LOGGING=n
```

ZMK標準USB HIDとは同時に有効化できません。必要な場合は
`CONFIG_ZMK_BLE=y` とし、BLE側に標準ZMK keyboardを残せます。

## behaviorとkeymap

keymapのルートにbehavior nodeを1つ定義します。

```dts
/ {
    behaviors {
        iidx_btn: iidx_button {
            compatible = "zmk,behavior-iidx-button";
            #binding-cells = <1>;
        };
    };
};
```

binding parameterは16 button field内のbit番号です。bit 7を飛ばします。

```dts
bindings = <
    &iidx_btn 0  &iidx_btn 1  &iidx_btn 2  &iidx_btn 3
    &iidx_btn 4  &iidx_btn 5  &iidx_btn 6
    &iidx_btn 8  &iidx_btn 9  &iidx_btn 10 &iidx_btn 11
>;
```

| binding | report上のボタン |
| --- | --- |
| `&iidx_btn 0` .. `6` | 1～7鍵 |
| `&iidx_btn 8` .. `11` | E1～E4 |

7および12～15はreserved bitを常に0に保つため受け付けません。

## X/Y API

encoderや独自input listenerからは `<zmk_iidx/hid.h>` をincludeして呼び出せます。

```c
zmk_iidx_hid_set_axis(ZMK_IIDX_AXIS_X, 32);
zmk_iidx_hid_adjust_axis(ZMK_IIDX_AXIS_Y, -1);
zmk_iidx_hid_set_axes(0, 0);
```

範囲外の値は `-127..127` へclampされます。

## 注意

- このモジュールはZephyrの旧USB device stack APIを使います。
- `HID_0` のみを使う前提で、提示された1-interface構成を保ちます。
- USB product/manufacturer文字列も参照機器と合わせる場合は、Zephyrの
  `CONFIG_USB_DEVICE_PRODUCT` / `CONFIG_USB_DEVICE_MANUFACTURER` も `.conf` で指定してください。
