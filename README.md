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

必要なもの: GLFW 3, libepoxy, GLM, Bullet, stb_image（音楽再生には SDL3_mixer または SDL2_mixer）、OpenGL 3.2 core profile

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


## 動画出力（AVI出力）

MMD の「AVI出力」と同じように、モーションを1コマずつ描いて動画や連番画像に書き出せます。
時間は実時間ではなくフレーム単位で進み（物理演算も 1/60 秒ずつ）、カメラ・照明モーション、影、複数モデルは画面表示と同じに描かれます。操作パネル（GUI）は入りません。

コマンドラインでは `--render` を付けると、ウィンドウを表示せずに書き出して終了します。

    $ pmx --render out.mp4 -s music.wav -c camera.vmd model.pmx motion.vmd
    $ pmx --render out.mp4 --range 300:899 --size 1920x1080 --fps 60 model.pmx motion.vmd
    $ pmx --render frames/ --range 0:149 model.pmx motion.vmd      # frames/00000.png, 00001.png, …

* `--render ファイル`: 出力先。拡張子が `.png`（`out.png` → `out_00000.png`, …。`out_%04d.png` のような書式も可）か、`/` で終わるディレクトリなら PNG 連番、`.mp4` `.webm` `.mkv` `.mov` `.avi` `.gif` などなら ffmpeg で動画に変換します（ffmpeg が必要）。
* `--range 開始:終了`: 書き出す VMD のフレーム範囲（両端を含む。既定はモーションの最初から最後のキーまで）。`300:` や `:899` のように片方を省略できます。長さは (終了−開始+1)/30 秒です。
* `--size 幅x高さ`: 画像サイズ（既定 1280x720。動画では偶数に丸めます）。
* `--fps 30|60`: フレームレート（既定 30）。
* `-s` で音楽を指定すると、動画に音声も入ります（`--range` の開始位置から。`--no-music` で入れない）。
* 開始フレームが 0 でないときは、物理演算が通しで再生したときと同じになるよう、フレーム 0 から開始位置までモーションと物理演算を進めてから書き出します。
* ウィンドウは表示しませんが X ディスプレイは必要です。サーバーでは `xvfb-run pmx --render …` のように使ってください。

ウィンドウからは、再生バーの「ファイル」→「動画出力…」で範囲（「現在位置から」ボタンもあります）・サイズ・フレームレート・出力先・形式を指定して書き出せます。書き出し中は進み具合と書き出し中の画面が表示され、「中止」で止められます（動画ファイルは削除、PNG はそれまでの分が残ります）。
