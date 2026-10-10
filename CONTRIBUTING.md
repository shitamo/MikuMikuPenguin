# How to contribute

第三者による貢献がこれからのMikuMikuPenguin(以下MMP)の開発には必要です。
我々は、皆さんの環境でMMPが動くようにする変更の貢献を出来るだけ簡単にできるようにしたいと考えています。
その為に、貢献しようとする人は以下の方針に従うよう、お願いします。

## Getting Started

* MMPに貢献する為には[GitHubアカウント](https://github.com/signup/free)が必要です。
* 問題にはIssueを作ってください（まだIssueが作られていない場合）。
  * バグをissueで報告する場合、バグを再現する為の説明を書いてください。
  * Issueのタイトルは英語が推奨ですが日本語でも構いません。
  * 本文は母国語で構いません。日本の方は日本語でお願いします。
* GitHubでレポジトリのforkを作ってください。

## Making Changes

* コードやドキュメントをMMPに貢献するにはベースとなるブランチから、トピック・ブランチを作ってください。
  * 通常、これ(ベースとなるブランチ)はmasterブランチです。
  * masterブランチから、トピック・ブランチを作るには： `git branch
    my_contribution master` してから `git
    checkout my_contribution`で新しいブランチに切り替えます。masterブランチ上で作業するのを避けてください。
    間違って、masterブランチ上で作業しないように、[pre-commit hookでmasterへのcommitを禁止した](http://blog.n-z.jp/blog/2014-02-07-pre-commit-hook.html) のような仕掛け
    をいれるのも良いでしょう。
* commitは合理的(ロジック単位)に分けてください。
* commitする前に、不要な空白は `git diff --check` で確認してください。
* commitメッセージが正しいフォーマットにあることを確認してください。commitメッセージは出来るだけ英語でお願いします。

````
必須なコミット情報はここに（出来れば英語で）。

上にある最初の列はパッチ/コミットの概要を説明します。
この本文はパッチが無い状態のプログラムの行動、なんでこの行動は問題なのか、どうやってパッチが問題を解決するのかを説明します。
````

* 変更の為にテストが必要ならそのテストが追加されているよう確認してください。
* 「make check」で、あなたが行った変更でソースツリーが壊れなかったか確認してください。他にテストがあればそれも実行してみてください。

## Coding Style
以下のようなスタイルでコードを書いてくださるようお願いします。
このスタイルに合わせなくても結構ですが、推奨です。
```cpp
int variableName;

class ClassName
{
	public:
	ClassName();
	~ClassName();

	void classFunctionName();

	private:
	int classVariableName;
};

void functionName()
{
	//function contents...
}
```

変数名と関数名には「variable_name」より「variableName」を使います。(基本はローワー・キャメル・ケースであり、スネークケースではない)

クラス名の頭文字 (ClassNameの「C」) は大文字です。(Classは、アッパーキャメルケースです)

ブラケットは開き括弧も閉じ括弧も行を分けて書いて下さい。次の例 **ではなく**:
```cpp
void functionName() { //ブラケットがクラス名や関数名と同じ行で開かれている例です。MMPのコードでは使われません。
	//Function contents here...
};
```

コメントはcommitメッセージと違って、英語でも日本語でも構いません。英語に自信ない方は日本語を使ってください。
理由としては、コメントで間違った英語を使うと英語圏の人でも日本語圏の人でも、どっちでも理解出来ないコメントになってしまう恐れがあるからです。
そういう場合は日本語圏の人にコードを理解して貰うだけでもまだマシだと思いますので、自信がある言語でコメントを書いてくださるようお願いします。

## Making Trivial Changes

### Documentation

コメントやドキュメンテーションのマイナーな変更には、commitメッセージを
「(doc)」で始めることをお勧めします。

````
(doc) Add documentation commit example to CONTRIBUTING

There is no example for contributing a documentation commit
to the MMP repository. This is a problem because the contributor
is left to assume how a commit of this nature may appear.

The first line summarizes the patch content, starting with '(doc)'
at the beginning of the commit. The body describes the nature of
the new documentation or comments added.
````

## Submitting Changes

* 自分のforkで、変更をトピック・ブランチにpushしてください。
* MikuMikuPenguinのレポジトリにpull requestを投稿してください。

# Additional Resources

* [General GitHub documentation](http://help.github.com/)
* [GitHub pull request documentation](http://help.github.com/send-pull-requests/)
* sn0w75に連絡: (http://twitter.com/sn0w75), メール (ibenrunnin@gmail.com)

