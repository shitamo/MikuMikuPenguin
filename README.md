**WARNING**
THIS FILE IS ENCODED IN UTF-8, NOT SJIS!!!

**注意**
このファイルはSJISではなくUTF-8でエンコードされています！！！

English translation is available in [README.en.md](README.en.md).

# LibMikuMikuPenguin(libmmp) v0.2 リードミ（日本語）

どうも初めまして。sn0w75です。

自分の好きなOSでミクミクしたくて、それを出来る為のライブラリやプログラムを作ってみました。
まだバグだらけですし、APIが確定しているわけでもないのでMMPをライブラリとして使うのはまだまだ早いかと思います。
ですが、これを起点として、Windows以外のOSでのMMDの開発が段々進めていったら～と思ってます。

（以上は原作者 sn0w75 氏による元の説明です。元のコード: https://github.com/sn0w75/MikuMikuPenguin/ ）

## 2026年 現行環境対応版について

このツリーは v0.2.1 をベースに、現在の Linux で動くよう次の変更を加えたものです。

* GLFW 2 → GLFW 3 へ移行、新しい GLM (0.9.9 / 1.x) に対応
* OpenGL ローダーを GLEW から libepoxy へ、画像読み込みを SOIL から stb_image へ、音楽再生は SDL3_mixer に対応（無ければ SDL2_mixer）
* ボーン行列をテクスチャバッファで渡すように変更（ボーン数 251 本の上限を撤廃）
* 頂点数 32767 超のモデルでのクラッシュ、テクスチャ番号 -1 などのクラッシュを修正
* テクスチャ名の大文字小文字を区別せずに検索
* 再生速度をモニタのリフレッシュレートに依存しないよう修正
* ビルドシステムを autotools から Meson に変更
* Debian パッケージ（debian/）を追加
* MME（MikuMikuEffect）風の画面エフェクト（ブルーム、発光、被写界深度、SSAO、色調補正、FXAA）。下の「画面エフェクト」を参照

必要なもの: GLFW 3, libepoxy, GLM, Bullet, stb_image（音楽再生には SDL3_mixer または SDL2_mixer）、OpenGL 3.2 core profile

## 画面エフェクト（MME 風）

MMD の MME でよく使われる見た目を、.fx ファイルは読まずにビューア自身の機能として OpenGL で描きます。
エフェクトを使うと、シーンをいったん浮動小数点のオフスクリーンバッファ（ウィンドウと同じ MSAA、深度付き）に描き、全画面の処理を重ねてから画面に出します。
操作パネル（GUI）はその上に描くので、エフェクトの影響を受けません。

| エフェクト | 名前 | 内容 | 主な設定 |
|---|---|---|---|
| ブルーム | `bloom` | 明るい部分（輝度がしきい値以上）が周りににじみます | 強さ、しきい値、広がり |
| 発光（AutoLuminous 風） | `glow` | 反射強度（スペキュラ係数）が 110 以上の材質が光ります（AutoLuminous と同じ決まり。強いほど明るく光る）。「高輝度部分も光らせる」がオンなら、モデルのうち特に明るい部分も光ります | 強さ、高輝度部分のしきい値 |
| 被写界深度 | `dof` | ピント位置から離れた所をぼかします。既定ではカメラの注視点（カメラモーションの注視点、自由カメラでは回転の中心）にピントが合います | ピント位置（手動時）、ぼけの強さ、最大ぼけ量 |
| SSAO | `ssao` | 物陰やくぼみ（髪の下、首、腕の付け根など）を暗くします | 強さ、半径（MMD の長さの単位） |
| 色調補正 | `color` | 明るさ・コントラスト・彩度・ガンマ | 4 つの値 |
| FXAA | `fxaa` | 輪郭のギザギザを滑らかにします | — |

* 初期状態はすべてオフです。オフのときは今までどおりウィンドウに直接描くので、見た目も速さも変わりません。
* 設定パネル（再生バーの「設定」）の「エフェクト」で、効果ごとのオン／オフと強さを変えられます。
* コマンドラインからも指定できます。`名前=値` で主な強さ（ブルーム・発光の強さ、被写界深度のぼけの強さ、SSAO の強さ）も設定できます。

      $ pmx --effect bloom,dof miku.pmx dance.vmd -c camera.vmd
      $ pmx --effect bloom=1.2,glow,ssao,fxaa miku.pmx dance.vmd
      $ pmx --dof-focus 30 miku.pmx            # ピント位置を 30 に固定（被写界深度をオン）
      $ pmx --color 0.05,1.1,1.2,1.0 miku.pmx  # 明るさ,コントラスト,彩度,ガンマ（色調補正をオン）

* GPU の負荷を抑えるため、SSAO・被写界深度のぼかし・ブルームは半分以下の解像度で計算し、使っていない効果の処理はしません。発光用のバッファは発光がオンのときだけ確保します。
* エフェクトのシェーダーは `shaders/postfx/`（インストール先では `/usr/share/mikumikupenguin/shaders/postfx/`）にあります。

## ディレクトリ構成

中身はこんな感じです：

* COPYING: このコードのライセンス
* doc: 旧版の説明書（HTML）が入ってます。
* include: LibMikuMikuPenguinのヘッダ（.h）ファイルがここに入ってます。
* libmmp: LibMikuMikuPenguinのソース（.cpp）ファイルがここに入ってます。
* shaders: PMXViewerのシェーダーがここに入ってます。
* src: PMXViewerのソースコード（.cpp、.h）がここに入ってます。
* data: 必要なテクスチャ等が入ってます。

## 依存ライブラリ

LibMikuMikuPenguinには GLFW 3、libepoxy、GLM、Bullet、iconv が必要です。

PMXViewer（PMX/PMDモデル対応）には libmmp と stb_image（libstb）が必要です。次のものは任意で、なくてもコンパイル出来ます。

* 音楽再生: SDL3_mixer（無ければ SDL2_mixer）
* 操作パネル（GUI）: Dear ImGui（libimgui-dev）、日本語フォントの検索に fontconfig

## インストール
ビルドには Meson と Ninja を使います（2026年版で autotools から変更）。

    $ meson setup build
    $ meson compile -C build
    $ meson test -C build        # 単体テスト

システムにインストールする場合は：

    $ sudo meson install -C build

音楽再生や操作パネルを無効にするには `meson setup build -Dsound=disabled -Dgui=disabled` とします。
Debian パッケージは `dpkg-buildpackage -us -uc -b` で作れます。

