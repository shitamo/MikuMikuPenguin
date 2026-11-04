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
    $ meson test -C build        # テスト（下記）

システムにインストールする場合は：

    $ sudo meson install -C build

音楽再生や操作パネルを無効にするには `meson setup build -Dsound=disabled -Dgui=disabled` とします。
Debian パッケージは `dpkg-buildpackage -us -uc -b` で作れます。

## テスト

`meson test -C build` で次のテストが走ります（GitHub Actions でも push とプルリクエストのたびに実行されます）。
テスト用のモデルとモーションは `test/gen_testdata.py` がビルドディレクトリに生成します（Python 3 が必要）。

* `unit` スイート: PMX / PMD / VMD の読み込み（壊れたファイルの検出を含む）、ボーン・モーフ・カメラ・照明の補間、シークと通常再生の一致、IK と付与の計算順
* `render` スイート: `pmx --snapshot フレーム 出力.png` で決まったフレームを描き、`test/reference/` の基準画像と比べます（多少の差は許容）。
  X が無ければ Xvfb を自動で起動し、Mesa のソフトウェア描画（`LIBGL_ALWAYS_SOFTWARE=1`）を使います。Xvfb も無ければスキップします。

片方だけ実行するには `meson test -C build --suite unit` のようにします。描画を意図して変えたときは

    $ meson compile -C build update-reference-images

で基準画像を作り直し、`test/reference/*.png` を目で確かめてからコミットしてください。
失敗したときの画像と差分（違う画素が赤）は `build/test/render/` に残ります。

