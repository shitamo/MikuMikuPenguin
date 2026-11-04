//On-screen controls (Dear ImGui): a playback bar at the bottom of the window
//and a settings panel, so that the viewer can be used without remembering
//the keyboard shortcuts.  Built only when Dear ImGui is available; the
//stubs at the end are used otherwise.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "viewer.h"
#include "motioncontroller.h"
#include "glfw_func_callbacks.h"
#include "sound.h"
#include "videowriter.h"

#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cstdio>

using namespace std;

#ifdef MMP_GUI

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#ifdef MMP_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

#include <algorithm>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <strings.h>
#include <climits>

namespace
{
	bool japanese=false; //Japanese labels (LANG=ja* and a font with Japanese glyphs)

	const char *T(const char *en, const char *ja)
	{
		return japanese ? ja : en;
	}

	bool localeIsJapanese()
	{
		const char *vars[]={"LC_ALL", "LC_MESSAGES", "LANG"};
		for(const char *v : vars)
		{
			const char *val=getenv(v);
			if(val && *val) return strncmp(val, "ja", 2)==0;
		}
		return false;
	}

	//A font file with Japanese glyphs (model and motion names are Japanese)
	bool findJapaneseFont(string &path, int &index)
	{
#ifdef MMP_FONTCONFIG
		if(!FcInit()) return false;
		FcPattern *pat=FcNameParse((const FcChar8*)"sans-serif:lang=ja");
		FcConfigSubstitute(NULL, pat, FcMatchPattern);
		FcDefaultSubstitute(pat);
		FcResult result;
		FcPattern *match=FcFontMatch(NULL, pat, &result);
		bool ok=false;
		if(match)
		{
			FcChar8 *file=NULL;
			FcCharSet *cs=NULL;
			int idx=0;
			if(FcPatternGetString(match, FC_FILE, 0, &file)==FcResultMatch &&
			   FcPatternGetCharSet(match, FC_CHARSET, 0, &cs)==FcResultMatch &&
			   FcCharSetHasChar(cs, 0x3042) && FcCharSetHasChar(cs, 0x6F22)) //あ 漢
			{
				FcPatternGetInteger(match, FC_INDEX, 0, &idx);
				path=(const char*)file;
				index=idx;
				ok=true;
			}
			FcPatternDestroy(match);
		}
		FcPatternDestroy(pat);
		return ok;
#else
		//common locations when fontconfig is not available
		static const char *candidates[]={
			"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
			"/usr/share/fonts/opentype/ipaexfont-gothic/ipaexg.ttf",
			"/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf",
			"/usr/share/fonts/truetype/vlgothic/VL-Gothic-Regular.ttf",
			"/usr/share/fonts/truetype/takao-gothic/TakaoPGothic.ttf",
		};
		for(const char *c : candidates)
		{
			FILE *f=fopen(c, "rb");
			if(f) { fclose(f); path=c; index=0; return true; }
		}
		return false;
#endif
	}

	string baseName(const string &path)
	{
		size_t slash=path.find_last_of('/');
		return slash==string::npos ? path : path.substr(slash+1);
	}

	//***Simple file browser (Dear ImGui has none)
	struct FileBrowser
	{
		bool open=false;
		bool requestOpen=false;
		PendingLoad::Kind kind=PendingLoad::Model;
		int model=-1;
		string title;
		vector<string> exts;  //lower case, without dot
		bool showAll=false;
		string dir;
		string selected;
		struct Entry { string name; bool isDir; };
		vector<Entry> entries;
		bool dirty=true;
		char pathBuf[PATH_MAX]={0};
	};
	FileBrowser fb;
	string lastDir; //where the previous file was chosen
	
	string currentDir()
	{
		char buf[PATH_MAX];
		if(getcwd(buf, sizeof(buf))) return buf;
		return "/";
	}
	
	string parentDir(const string &dir)
	{
		if(dir.size()<=1) return "/";
		size_t slash=dir.find_last_of('/', dir.size()-2);
		if(slash==string::npos || slash==0) return "/";
		return dir.substr(0, slash);
	}
	
	string joinPath(const string &dir, const string &name)
	{
		return (dir.empty() || dir.back()=='/') ? dir+name : dir+"/"+name;
	}
	
	bool isDirectory(const string &path)
	{
		struct stat st;
		return stat(path.c_str(), &st)==0 && S_ISDIR(st.st_mode);
	}
	
	bool hasExt(const string &name, const vector<string> &exts)
	{
		size_t dot=name.rfind('.');
		if(dot==string::npos) return false;
		string e=name.substr(dot+1);
		for(char &c : e) c=tolower((unsigned char)c);
		return std::find(exts.begin(), exts.end(), e)!=exts.end();
	}
	
	void scanDir(FileBrowser &b)
	{
		b.entries.clear();
		b.dirty=false;
		DIR *d=opendir(b.dir.c_str());
		if(!d) return;
		while(dirent *e=readdir(d))
		{
			string name=e->d_name;
			if(name=="." || name==".." || name[0]=='.') continue;
			bool dirEntry=isDirectory(joinPath(b.dir, name));
			if(!dirEntry && !b.showAll && !hasExt(name, b.exts)) continue;
			b.entries.push_back(FileBrowser::Entry{name, dirEntry});
		}
		closedir(d);
		std::sort(b.entries.begin(), b.entries.end(), [](const FileBrowser::Entry &x, const FileBrowser::Entry &y)
		{
			if(x.isDir!=y.isDir) return x.isDir;
			return strcasecmp(x.name.c_str(), y.name.c_str())<0;
		});
	}
	
	void openBrowser(PendingLoad::Kind kind, int model, const char *title, const vector<string> &exts)
	{
		fb.requestOpen=true;
		fb.kind=kind;
		fb.model=model;
		fb.title=title;
		fb.exts=exts;
		fb.selected.clear();
		if(fb.dir.empty()) fb.dir=lastDir.empty() ? currentDir() : lastDir;
		fb.dirty=true;
	}
	
	const vector<string> modelExts={"pmx", "pmd"};
	const vector<string> vmdExts={"vmd"};
	const vector<string> musicExts={"wav", "ogg", "mp3", "flac", "opus", "mid", "midi", "mod", "xm", "it", "s3m", "aif", "aiff", "voc", "wv"};
	
	//***Video output dialog
	struct ExportDialog
	{
		bool requestOpen=false;
		int start=0, end=0;
		int size[2]={1280, 720};
		int fps=30;
		bool music=true;
		char path[PATH_MAX]={0};
	};
	ExportDialog ed;
	
	string pathExtension(const string &p)
	{
		size_t slash=p.find_last_of('/'), dot=p.find_last_of('.');
		if(dot==string::npos || (slash!=string::npos && dot<slash)) return "";
		string e=p.substr(dot+1);
		for(char &c : e) c=(char)tolower((unsigned char)c);
		return e;
	}
	
	//replace (or add) the extension of the output file
	void setExtension(char *buf, size_t size, const char *ext)
	{
		string p=buf;
		if(!p.empty() && p.back()=='/') p+="frame";
		size_t slash=p.find_last_of('/'), dot=p.find_last_of('.');
		if(dot!=string::npos && (slash==string::npos || dot>slash)) p=p.substr(0, dot);
		p+=string(".")+ext;
		snprintf(buf, size, "%s", p.c_str());
	}
	
	string timeText(int frame)
	{
		char buf[32];
		int cs=frame*100/30; //hundredths of a second (30 fps)
		snprintf(buf, sizeof(buf), "%d:%02d.%02d", cs/6000, (cs/100)%60, cs%100);
		return buf;
	}
}

void Viewer::initGUI()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io=ImGui::GetIO();
	io.IniFilename=NULL; //do not write imgui.ini into the current directory

	float scale=1.0f;
	{
		float xs=1.0f, ys=1.0f;
		glfwGetWindowContentScale(window, &xs, &ys);
		if(xs>0.5f && xs<4.0f) scale=xs;
	}

	string fontPath;
	int fontIndex=0;
	bool haveFont=findJapaneseFont(fontPath, fontIndex);
	if(haveFont)
	{
		ImFontConfig cfg;
		cfg.FontNo=fontIndex;
		cfg.OversampleH=1;
		//Japanese + arrows (←↑→↓ are used in the labels)
		static ImVector<ImWchar> ranges;
		ImFontGlyphRangesBuilder builder;
		builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
		static const ImWchar extra[]={0x2000, 0x206F, 0x2190, 0x21FF, 0x25A0, 0x25FF, 0}; //punctuation (…), arrows, shapes
		builder.AddRanges(extra);
		builder.BuildRanges(&ranges);
		ImFont *font=io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 17.0f*scale, &cfg, ranges.Data);
		if(!font) haveFont=false;
	}
	if(!haveFont)
	{
		ImFontConfig cfg;
		cfg.SizePixels=13.0f*scale;
		io.Fonts->AddFontDefault(&cfg);
	}
	japanese=haveFont && localeIsJapanese();

	ImGui::StyleColorsDark();
	ImGuiStyle &style=ImGui::GetStyle();
	style.WindowRounding=6.0f;
	style.FrameRounding=4.0f;
	style.ScaleAllSizes(scale);

	//install_callbacks: the backend chains the viewer's own GLFW callbacks
	if(!ImGui_ImplGlfw_InitForOpenGL(window, true) || !ImGui_ImplOpenGL3_Init("#version 150"))
	{
		cerr<<"WARNING: GUI initialisation failed; continuing without it"<<endl;
		ImGui::DestroyContext();
		guiAvailable=false;
		return;
	}
	guiAvailable=true;
	glUseProgram(shaderProgram);
}

void Viewer::shutdownGUI()
{
	if(!guiAvailable) return;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	guiAvailable=false;
}

void Viewer::guiFrame()
{
	if(!guiAvailable) return;
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if(guiVisible)
	{
		const ImGuiViewport *vp=ImGui::GetMainViewport();
		const ImGuiStyle &style=ImGui::GetStyle();
		int frame=std::min(currentTime(), std::max(endTime, 0))/2;
		int endFrame=std::max(endTime/2, 0);

		//***Playback bar***
		float lineH=ImGui::GetFrameHeight();
		float barH=lineH*2.0f+style.ItemSpacing.y+style.WindowPadding.y*2.0f;
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y+vp->WorkSize.y-barH));
		ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, barH));
		ImGui::SetNextWindowBgAlpha(0.65f);
		ImGuiWindowFlags barFlags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
		                          ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing;
		if(ImGui::Begin("##playback", NULL, barFlags))
		{
			//timeline: drag to seek
			ImGui::SetNextItemWidth(-1.0f);
			int sliderFrame=frame;
			char fmt[64];
			snprintf(fmt, sizeof(fmt), "%%d / %d", endFrame);
			ImGui::BeginDisabled(endFrame==0);
			if(ImGui::SliderInt("##timeline", &sliderFrame, 0, endFrame>0 ? endFrame : 1, fmt) && sliderFrame!=frame)
			{
				seekTo(sliderFrame*2);
			}
			ImGui::EndDisabled();
			if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Drag to seek", "ドラッグでシーク"));

			auto button=[&](const char *label, const char *tip)->bool
			{
				bool pressed=ImGui::Button(label);
				if(tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
				ImGui::SameLine();
				return pressed;
			};
			if(button("|<", T("Beginning (Home)", "先頭へ (Home)"))) seekTo(0);
			if(button("<<", T("Back 1 s (Left, Shift: 10 s)", "1秒戻る (←, Shift+←で10秒)")))
				seekTo(currentTime()-(ImGui::GetIO().KeyShift ? 600 : 60));
			if(button("<", T("Previous frame (,)", "1コマ戻る (,)"))) stepFrame(-1);
			{
				//fixed width so that the bar does not jump when the label changes
				const char *label=paused ? T("Play", "再生") : T("Pause", "一時停止");
				float w=std::max(ImGui::CalcTextSize(T("Pause", "一時停止")).x, ImGui::CalcTextSize(T("Play", "再生")).x)+style.FramePadding.x*2.0f;
				if(ImGui::Button(label, ImVec2(w, 0.0f))) setPaused(!paused);
				if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Play / pause (Space)", "再生／一時停止 (Space)"));
				ImGui::SameLine();
			}
			if(button(">", T("Next frame (.)", "1コマ進む (.)"))) stepFrame(+1);
			if(button(">>", T("Forward 1 s (Right, Shift: 10 s)", "1秒進む (→, Shift+→で10秒)")))
				seekTo(currentTime()+(ImGui::GetIO().KeyShift ? 600 : 60));
			if(button(">|", T("End (End)", "末尾へ (End)"))) seekTo(endTime);

			ImGui::Checkbox(T("Loop", "ループ"), &loopPlayback);
			if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Restart at the end (O)", "最後まで来たら先頭から (O)"));
			ImGui::SameLine();
			ImGui::TextUnformatted((timeText(frame)+" / "+timeText(endFrame)).c_str());

			//right-aligned: file / settings / hide
			const char *fileLabel=T("File", "ファイル");
			const char *settingsLabel=T("Settings", "設定");
			const char *hideLabel=T("Hide (Tab)", "隠す (Tab)");
			float rightW=ImGui::CalcTextSize(fileLabel).x+ImGui::CalcTextSize(settingsLabel).x+ImGui::CalcTextSize(hideLabel).x+style.FramePadding.x*6.0f+style.ItemSpacing.x*2.0f;
			ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x-rightW));
			if(ImGui::Button(fileLabel)) ImGui::OpenPopup("filemenu");
			if(ImGui::BeginPopup("filemenu"))
			{
				if(ImGui::MenuItem(T("Add model (PMX/PMD)...", "モデルを追加 (PMX/PMD)…")))
					openBrowser(PendingLoad::Model, -1, T("Add model", "モデルを追加"), modelExts);
				if(ImGui::BeginMenu(T("Motion for", "モーションを読み込む"), !models.empty()))
				{
					for(size_t i=0; i<models.size(); ++i)
					{
						string label=std::to_string(i+1)+": "+(models[i]->pmxInfo->modelName.empty() ? baseName(models[i]->spec.modelPath) : models[i]->pmxInfo->modelName);
						if(ImGui::MenuItem(label.c_str()))
							openBrowser(PendingLoad::Motion, (int)i, T("Motion", "モーション"), vmdExts);
					}
					ImGui::EndMenu();
				}
				if(ImGui::MenuItem(T("Camera motion (VMD)...", "カメラモーション (VMD)…")))
					openBrowser(PendingLoad::Camera, -1, T("Camera motion", "カメラモーション"), vmdExts);
				if(ImGui::MenuItem(T("Light motion (VMD)...", "照明モーション (VMD)…")))
					openBrowser(PendingLoad::Light, -1, T("Light motion", "照明モーション"), vmdExts);
				if(ImGui::MenuItem(T("Music...", "音楽…")))
					openBrowser(PendingLoad::Music, -1, T("Music", "音楽"), musicExts);
				ImGui::Separator();
				if(ImGui::MenuItem(T("Remove camera motion", "カメラモーションを外す"), NULL, false, !cameraPathShown.empty()))
					pendingLoads.push_back(PendingLoad{PendingLoad::Camera, "", -1});
				if(ImGui::MenuItem(T("Remove light motion", "照明モーションを外す"), NULL, false, !lightPathShown.empty()))
					pendingLoads.push_back(PendingLoad{PendingLoad::Light, "", -1});
				if(ImGui::MenuItem(T("Stop music", "音楽を外す"), NULL, false, !musicPathShown.empty()))
					pendingLoads.push_back(PendingLoad{PendingLoad::Music, "", -1});
				ImGui::Separator();
				if(ImGui::MenuItem(T("Export video...", "動画出力…"), NULL, false, !models.empty()))
				{
					ed.requestOpen=true;
					setPaused(true); //"From here" uses the current position
					ed.start=0;
					ed.end=endFrame;
					if(!ed.path[0])
					{
						string dir=lastDir.empty() ? currentDir() : lastDir;
						snprintf(ed.path, sizeof(ed.path), "%s", joinPath(dir, VideoWriter::ffmpegAvailable() ? "pmx_video.mp4" : "pmx_video.png").c_str());
					}
				}
				ImGui::Separator();
				ImGui::TextDisabled("%s", T("Files can also be dropped onto the window", "ファイルはウィンドウへのドロップでも読み込めます"));
				ImGui::EndPopup();
			}
			ImGui::SameLine();
			if(ImGui::Button(settingsLabel)) guiShowSettings=!guiShowSettings;
			ImGui::SameLine();
			if(ImGui::Button(hideLabel)) guiVisible=false;
		}
		ImGui::End();

		//***Settings panel***
		if(guiShowSettings)
		{
			ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x+10.0f, vp->WorkPos.y+10.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowBgAlpha(0.8f);
			if(ImGui::Begin(T("Settings###settings", "設定###settings"), &guiShowSettings, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings))
			{
				if(ImGui::CollapsingHeader(T("Files", "ファイル"), ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto fileRow=[&](const char *label, const string &path, const string &extra)
					{
						if(path.empty()) return;
						ImGui::Text("%s: %s", label, baseName(path).c_str());
						if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", path.c_str());
						if(!extra.empty()) { ImGui::SameLine(); ImGui::TextDisabled("(%s)", extra.c_str()); }
					};
					//one block per model: files, show/hide, position
					for(size_t i=0; i<models.size(); ++i)
					{
						SceneModel &m=*models[i];
						ImGui::PushID((int)i);
						ImGui::Checkbox("##visible", &m.visible);
						if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Show this model", "このモデルを表示"));
						ImGui::SameLine();
						ImGui::Text("%s: %s", T("Model", "モデル"), baseName(m.spec.modelPath).c_str());
						if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", m.spec.modelPath.c_str());
						if(!m.pmxInfo->modelName.empty()) { ImGui::SameLine(); ImGui::TextDisabled("(%s)", m.pmxInfo->modelName.c_str()); }
						ImGui::Indent();
						fileRow(T("Motion", "モーション"), m.spec.motionPath.empty() ? string(T("(none)", "（なし）")) : m.spec.motionPath, "");
						if(ImGui::SmallButton(T("Load motion...", "モーション読み込み…")))
							openBrowser(PendingLoad::Motion, (int)i, T("Motion", "モーション"), vmdExts);
						if(!m.spec.motionPath.empty())
						{
							ImGui::SameLine();
							if(ImGui::SmallButton(T("Remove motion", "モーションを外す")))
								pendingLoads.push_back(PendingLoad{PendingLoad::Motion, "", (int)i});
						}
						ImGui::SameLine();
						if(ImGui::SmallButton(T("Remove model", "モデルを削除")))
							pendingLoads.push_back(PendingLoad{PendingLoad::Remove, "", (int)i});
						if(models.size()>1 || m.offset!=glm::vec3(0.0f))
						{
							//shown in MMD coordinates (Z is mirrored)
							float pos[3]={m.offset.x, m.offset.y, -m.offset.z};
							ImGui::SetNextItemWidth(ImGui::CalcTextSize("0000000").x*3.0f);
							if(ImGui::DragFloat3(T("Position", "位置"), pos, 0.05f, -1000.0f, 1000.0f, "%.2f"))
							{
								m.offset=glm::vec3(pos[0], pos[1], -pos[2]);
								updateSceneBounds();
							}
						}
						ImGui::Unindent();
						ImGui::PopID();
					}
					if(ImGui::Button(T("Add model...", "モデルを追加…")))
						openBrowser(PendingLoad::Model, -1, T("Add model", "モデルを追加"), modelExts);
					fileRow(T("Camera", "カメラ"), cameraPathShown, "");
					fileRow(T("Light", "照明"), lightPathShown, "");
					fileRow(T("Music", "音楽"), musicPathShown, "");
				}
				if(ImGui::CollapsingHeader(T("Display", "表示"), ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::BeginDisabled(cameraController.empty());
					ImGui::Checkbox(T("Camera motion (C)", "カメラモーション (C)"), &useCameraMotion);
					ImGui::EndDisabled();
					if(cameraController.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
						ImGui::SetTooltip("%s", T("No camera keys (use -c camera.vmd)", "カメラのキーがありません (-c カメラ.vmd で指定)"));

					ImGui::BeginDisabled(lightController.empty());
					ImGui::Checkbox(T("Light motion (L)", "照明モーション (L)"), &useLightMotion);
					ImGui::EndDisabled();

					ImGui::Checkbox(T("Ground shadow (Z)", "地面影 (Z)"), &useGroundShadow);
					ImGui::Checkbox(T("Self shadow (X)", "セルフシャドウ (X)"), &useSelfShadow);
					ImGui::Checkbox(T("Physics", "物理演算"), &physicsEnabled);
					ImGui::Checkbox(models.size()>1 ? T("Show models", "モデルを表示") : T("Show model", "モデルを表示"), &showModel);

					const char *debugItems[]={
						T("None", "なし"), T("Wireframe (S)", "ワイヤーフレーム (S)"), T("AABB (D)", "AABB (D)"),
						T("Constraints (F)", "ジョイント (F)"), T("Constraint limits (G)", "ジョイントの範囲 (G)")};
					ImGui::SetNextItemWidth(ImGui::CalcTextSize(debugItems[4]).x+ImGui::GetFrameHeight()*2.0f);
					ImGui::Combo(T("Rigid body display", "剛体の表示"), &debugDrawMode, debugItems, 5);

					float height=-modelTranslate.y;
					ImGui::SetNextItemWidth(ImGui::CalcTextSize("0000000000").x);
					if(ImGui::DragFloat(T("Model height (Up/Down)", "モデルの上下 (↑↓)"), &height, 0.05f, -100.0f, 100.0f, "%.2f"))
						modelTranslate.y=-height;

					if(ImGui::Button(T("Reset camera (R)", "カメラをリセット (R)")))
					{
						resetCamera();
						useCameraMotion=false;
					}
				}
				if(ImGui::CollapsingHeader(T("Effects", "エフェクト")))
				{
					EffectSettings &fx=postfx.settings;
					const float sliderWidth=ImGui::CalcTextSize("00000000000000").x;
					auto slider=[&](const char *label, float *v, float lo, float hi, const char *fmt="%.2f")
					{
						ImGui::SetNextItemWidth(sliderWidth);
						ImGui::SliderFloat(label, v, lo, hi, fmt);
					};
					//one block per effect: on/off, then its settings while it is on
					auto effect=[&](const char *label, bool *on, const char *tip)
					{
						ImGui::Checkbox(label, on);
						if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
						return *on;
					};
					if(postfx.failed())
					{
						ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", T("Effects are not available:", "エフェクトを使えません:"));
						ImGui::TextWrapped("%s", postfx.error().c_str());
					}
					ImGui::PushID("effects");
					if(effect(T("Bloom", "ブルーム"), &fx.bloom, T("Bright parts bleed into their surroundings", "明るい部分がにじみます")))
					{
						ImGui::Indent();
						slider(T("Intensity##bloom", "強さ##bloom"), &fx.bloomIntensity, 0.0f, 3.0f);
						slider(T("Threshold##bloom", "しきい値##bloom"), &fx.bloomThreshold, 0.0f, 0.99f);
						slider(T("Spread##bloom", "広がり##bloom"), &fx.bloomRadius, 0.0f, 1.0f);
						ImGui::Unindent();
					}
					if(effect(T("Glow (AutoLuminous-like)", "発光 (AutoLuminous風)"), &fx.glow,
					          T("Materials with specular power 110 or more emit light", "反射強度が 110 以上の材質が光ります")))
					{
						ImGui::Indent();
						slider(T("Intensity##glow", "強さ##glow"), &fx.glowIntensity, 0.0f, 5.0f);
						ImGui::Checkbox(T("Bright parts glow too", "高輝度部分も光らせる"), &fx.glowBright);
						if(fx.glowBright) slider(T("Threshold##glow", "しきい値##glow"), &fx.glowThreshold, 0.0f, 0.99f);
						ImGui::Unindent();
					}
					if(effect(T("Depth of field", "被写界深度"), &fx.dof, T("Blur away from the focus distance", "ピント位置から離れた所をぼかします")))
					{
						ImGui::Indent();
						ImGui::Checkbox(T("Focus on the camera's point of interest", "カメラの注視点にピントを合わせる"), &fx.dofAutoFocus);
						if(fx.dofAutoFocus)
						{
							ImGui::TextDisabled("%s %.1f", T("Focus distance:", "ピント位置:"), focusDistance);
						}
						else
						{
							ImGui::SetNextItemWidth(sliderWidth);
							ImGui::DragFloat(T("Focus distance", "ピント位置"), &fx.dofFocus, 0.1f, 0.1f, 1000.0f, "%.1f");
							ImGui::SameLine();
							if(ImGui::SmallButton(T("Current", "現在の注視点"))) fx.dofFocus=focusDistance;
						}
						slider(T("Strength##dof", "ぼけの強さ##dof"), &fx.dofStrength, 0.0f, 10.0f);
						slider(T("Max blur##dof", "最大ぼけ量##dof"), &fx.dofMaxBlur, 0.1f, 3.0f);
						ImGui::Unindent();
					}
					if(effect(T("SSAO", "SSAO (物陰を暗く)"), &fx.ssao, T("Darkens creases and places hidden from the open space", "物陰やくぼみを暗くします")))
					{
						ImGui::Indent();
						slider(T("Strength##ssao", "強さ##ssao"), &fx.ssaoStrength, 0.0f, 3.0f);
						slider(T("Radius##ssao", "半径##ssao"), &fx.ssaoRadius, 0.05f, 5.0f);
						ImGui::Unindent();
					}
					if(effect(T("Colour correction", "色調補正"), &fx.color, T("Brightness, contrast, saturation and gamma", "明るさ・コントラスト・彩度・ガンマ")))
					{
						ImGui::Indent();
						slider(T("Brightness", "明るさ"), &fx.brightness, -0.5f, 0.5f);
						slider(T("Contrast", "コントラスト"), &fx.contrast, 0.0f, 2.0f);
						slider(T("Saturation", "彩度"), &fx.saturation, 0.0f, 2.0f);
						slider(T("Gamma", "ガンマ"), &fx.gamma, 0.2f, 3.0f);
						if(ImGui::SmallButton(T("Reset##color", "リセット##color")))
						{
							EffectSettings d;
							fx.brightness=d.brightness; fx.contrast=d.contrast; fx.saturation=d.saturation; fx.gamma=d.gamma;
						}
						ImGui::Unindent();
					}
					effect("FXAA", &fx.fxaa, T("Smooths jagged edges", "輪郭のギザギザを滑らかにします"));
					if(fx.any() && ImGui::Button(T("All effects off", "エフェクトをすべてオフ")))
					{
						fx.bloom=fx.glow=fx.dof=fx.ssao=fx.color=fx.fxaa=false;
					}
					ImGui::PopID();
				}
				if(ImGui::CollapsingHeader(T("Keyboard shortcuts", "ショートカットキー")))
				{
					static const char *rowsEn[][2]={
						{"Space", "play / pause"}, {"Left / Right", "seek 1 s (Shift: 10 s)"},
						{", / .", "previous / next frame"}, {"Home / End", "beginning / end"},
						{"O", "loop"}, {"C", "camera motion"}, {"L", "light motion"},
						{"Z / X", "ground / self shadow"}, {"R", "reset camera"},
						{"Left drag / wheel", "rotate / zoom"}, {"Up / Down", "move model"},
						{"Tab", "show / hide controls"}, {"Esc", "quit"}};
					static const char *rowsJa[][2]={
						{"Space", "再生／一時停止"}, {"← / →", "1秒シーク (Shift: 10秒)"},
						{", / .", "1コマ戻る／進む"}, {"Home / End", "先頭／末尾"},
						{"O", "ループ"}, {"C", "カメラモーション"}, {"L", "照明モーション"},
						{"Z / X", "地面影／セルフシャドウ"}, {"R", "カメラをリセット"},
						{"左ドラッグ / ホイール", "回転／ズーム"}, {"↑ / ↓", "モデルの上下"},
						{"Tab", "操作パネルの表示／非表示"}, {"Esc", "終了"}};
					const char *(*rows)[2]=japanese ? rowsJa : rowsEn;
					if(ImGui::BeginTable("keys", 2, ImGuiTableFlags_SizingFixedFit))
					{
						for(int i=0; i<13; ++i)
						{
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(rows[i][0]);
							ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(rows[i][1]);
						}
						ImGui::EndTable();
					}
				}
			}
			ImGui::End();
		}
	}
	else
	{
		//a small hint so that the hidden controls can be found again
		const ImGuiViewport *vp=ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x+vp->WorkSize.x-8.0f, vp->WorkPos.y+vp->WorkSize.y-8.0f), 0, ImVec2(1.0f, 1.0f));
		ImGui::SetNextWindowBgAlpha(0.35f);
		if(ImGui::Begin("##showgui", NULL, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|
		                ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoMove))
		{
			if(ImGui::SmallButton(T("Controls (Tab)", "操作パネル (Tab)"))) guiVisible=true;
		}
		ImGui::End();
	}

	const ImGuiViewport *vpm=ImGui::GetMainViewport();
	ImVec2 centre(vpm->WorkPos.x+vpm->WorkSize.x*0.5f, vpm->WorkPos.y+vpm->WorkSize.y*0.5f);
	
	//***Nothing loaded yet***
	if(models.empty() && pendingLoads.empty() && !fb.open)
	{
		ImGui::SetNextWindowPos(centre, 0, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowBgAlpha(0.8f);
		if(ImGui::Begin("##empty", NULL, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove))
		{
			ImGui::TextUnformatted(T("Drop PMX/PMD, VMD and music files onto this window,", "PMX/PMD・VMD・音楽ファイルをこのウィンドウにドロップするか、"));
			ImGui::TextUnformatted(T("or choose them here:", "ここから選んでください。"));
			if(ImGui::Button(T("Add model...", "モデルを追加…")))
				openBrowser(PendingLoad::Model, -1, T("Add model", "モデルを追加"), modelExts);
		}
		ImGui::End();
	}
	
	//***File browser***
	if(fb.requestOpen)
	{
		ImGui::OpenPopup("###filebrowser");
		fb.requestOpen=false;
		fb.open=true;
	}
	ImGui::SetNextWindowSize(ImVec2(vpm->WorkSize.x*0.6f, vpm->WorkSize.y*0.7f), ImGuiCond_Appearing);
	ImGui::SetNextWindowPos(centre, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	string popupTitle=fb.title+"###filebrowser";
	if(ImGui::BeginPopupModal(popupTitle.c_str(), NULL, ImGuiWindowFlags_NoSavedSettings))
	{
		if(fb.dirty)
		{
			scanDir(fb);
			snprintf(fb.pathBuf, sizeof(fb.pathBuf), "%s", fb.dir.c_str());
		}
		string chosen;
		
		if(ImGui::Button(T("Up", "上へ"))) { fb.dir=parentDir(fb.dir); fb.dirty=true; }
		ImGui::SameLine();
		if(ImGui::Button(T("Home", "ホーム")))
		{
			const char *home=getenv("HOME");
			fb.dir=home ? home : "/";
			fb.dirty=true;
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		if(ImGui::InputText("##path", fb.pathBuf, sizeof(fb.pathBuf), ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll))
		{
			string p=fb.pathBuf;
			if(isDirectory(p)) { fb.dir=p; fb.dirty=true; }
			else chosen=p; //a file typed in directly
		}
		
		float footer=ImGui::GetFrameHeightWithSpacing()*2.0f;
		if(ImGui::BeginChild("##entries", ImVec2(0.0f, -footer), true))
		{
			for(const FileBrowser::Entry &e : fb.entries)
			{
				string label=e.isDir ? e.name+"/" : e.name;
				bool sel=(!e.isDir && e.name==fb.selected);
				if(ImGui::Selectable(label.c_str(), sel, ImGuiSelectableFlags_AllowDoubleClick))
				{
					if(e.isDir)
					{
						fb.dir=joinPath(fb.dir, e.name); //folders open with a single click
						fb.dirty=true;
						fb.selected.clear();
					}
					else
					{
						fb.selected=e.name;
						if(ImGui::IsMouseDoubleClicked(0)) chosen=joinPath(fb.dir, e.name);
					}
				}
			}
			if(fb.entries.empty()) ImGui::TextDisabled("%s", T("(no matching files)", "（該当するファイルがありません）"));
		}
		ImGui::EndChild();
		
		if(ImGui::Checkbox(T("Show all files", "すべてのファイルを表示"), &fb.showAll)) fb.dirty=true;
		ImGui::SameLine();
		ImGui::TextDisabled("%s", fb.selected.c_str());
		float buttonsW=ImGui::CalcTextSize(T("Open", "開く")).x+ImGui::CalcTextSize(T("Cancel", "キャンセル")).x+ImGui::GetStyle().FramePadding.x*4.0f+ImGui::GetStyle().ItemSpacing.x;
		ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x-buttonsW));
		ImGui::BeginDisabled(fb.selected.empty());
		if(ImGui::Button(T("Open", "開く"))) chosen=joinPath(fb.dir, fb.selected);
		ImGui::EndDisabled();
		ImGui::SameLine();
		bool cancel=ImGui::Button(T("Cancel", "キャンセル")) || ImGui::IsKeyPressed(ImGuiKey_Escape);
		
		if(!chosen.empty())
		{
			pendingLoads.push_back(PendingLoad{fb.kind, chosen, fb.model});
			lastDir=fb.dir;
			cancel=true;
		}
		if(cancel)
		{
			fb.open=false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	else
	{
		fb.open=false;
	}
	
	//***Video output: settings***
	if(ed.requestOpen)
	{
		ImGui::OpenPopup("###exportvideo");
		ed.requestOpen=false;
	}
	ImGui::SetNextWindowPos(centre, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if(ImGui::BeginPopupModal(T("Export video###exportvideo", "動画出力###exportvideo"), NULL, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings))
	{
		int endFrame=std::max(endTime/2, 0);
		float numW=ImGui::CalcTextSize("000000").x+ImGui::GetFrameHeight()*2.0f+ImGui::GetStyle().ItemInnerSpacing.x*2.0f;
		
		//range (VMD frames, both included)
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(T("Range (frames)", "範囲（フレーム）"));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(numW);
		ImGui::InputInt("##start", &ed.start);
		ImGui::SameLine();
		ImGui::TextUnformatted("-");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(numW);
		ImGui::InputInt("##end", &ed.end);
		ImGui::SameLine();
		if(ImGui::SmallButton(T("All", "全体"))) { ed.start=0; ed.end=endFrame; }
		ImGui::SameLine();
		if(ImGui::SmallButton(T("From here", "現在位置から"))) ed.start=std::min(currentTime(), std::max(endTime, 0))/2;
		ed.start=std::max(ed.start, 0);
		ed.end=std::max(ed.end, 0);
		
		//size
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(T("Size", "サイズ"));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(numW*2.0f);
		ImGui::InputInt2("##size", ed.size);
		ImGui::SameLine();
		static const int presets[][2]={{640,360},{1280,720},{1920,1080},{2560,1440},{3840,2160}};
		if(ImGui::SmallButton(T("Presets", "プリセット"))) ImGui::OpenPopup("sizepresets");
		if(ImGui::BeginPopup("sizepresets"))
		{
			for(const auto &p : presets)
			{
				string label=std::to_string(p[0])+" x "+std::to_string(p[1]);
				if(ImGui::MenuItem(label.c_str())) { ed.size[0]=p[0]; ed.size[1]=p[1]; }
			}
			int ww=0, wh=0;
			glfwGetFramebufferSize(window, &ww, &wh);
			string label=string(T("Window", "ウィンドウと同じ"))+" ("+std::to_string(ww)+" x "+std::to_string(wh)+")";
			if(ImGui::MenuItem(label.c_str())) { ed.size[0]=ww; ed.size[1]=wh; }
			ImGui::EndPopup();
		}
		ed.size[0]=std::min(std::max(ed.size[0], 16), 16384);
		ed.size[1]=std::min(std::max(ed.size[1], 16), 16384);
		
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(T("Frame rate", "フレームレート"));
		ImGui::SameLine();
		ImGui::RadioButton("30 fps", &ed.fps, 30);
		ImGui::SameLine();
		ImGui::RadioButton("60 fps", &ed.fps, 60);
		
		//output: the extension decides the format
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(T("Output", "出力先"));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(std::max(ImGui::CalcTextSize("0").x*48.0f, 300.0f));
		ImGui::InputText("##output", ed.path, sizeof(ed.path));
		bool ffmpeg=VideoWriter::ffmpegAvailable();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(T("Format", "形式"));
		ImGui::SameLine();
		string ext=pathExtension(ed.path);
		bool sequence=VideoWriter::isImageSequence(ed.path);
		ImGui::BeginDisabled(!ffmpeg);
		if(ImGui::RadioButton("MP4 (H.264)", ext=="mp4")) setExtension(ed.path, sizeof(ed.path), "mp4");
		ImGui::SameLine();
		if(ImGui::RadioButton("WebM (VP9)", ext=="webm")) setExtension(ed.path, sizeof(ed.path), "webm");
		ImGui::EndDisabled();
		ImGui::SameLine();
		if(ImGui::RadioButton(T("PNG sequence", "PNG 連番"), sequence)) setExtension(ed.path, sizeof(ed.path), "png");
		
		sequence=VideoWriter::isImageSequence(ed.path);
		if(sequence)
		{
			ImGui::TextDisabled(T("Files: %s, %s, ...", "ファイル: %s, %s, …"),
			                    baseName(VideoWriter::sequenceFileName(ed.path, 0)).c_str(), baseName(VideoWriter::sequenceFileName(ed.path, 1)).c_str());
		}
		else if(!ffmpeg)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", T("ffmpeg is not installed: only PNG files can be written", "ffmpeg が見つかりません（PNG 連番のみ出力できます）"));
		}
		else
		{
			ImGui::TextDisabled("%s", T("Encoded by ffmpeg (mp4, webm, mkv, mov, avi, gif ...)", "ffmpeg で変換します（mp4, webm, mkv, mov, avi, gif など）"));
		}
		if(!musicPathShown.empty())
		{
			ImGui::BeginDisabled(sequence);
			ImGui::Checkbox(T("Include the music", "音楽を入れる"), &ed.music);
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::TextDisabled("%s", baseName(musicPathShown).c_str());
		}
		
		bool valid=ed.end>=ed.start && ed.path[0] && (sequence || ffmpeg);
		if(valid)
		{
			int frames=(ed.end-ed.start+1)*(ed.fps/30);
			ImGui::Text(T("%d frames, %.2f s", "%d コマ、%.2f 秒"), frames, (ed.end-ed.start+1)/30.0);
		}
		else if(ed.end<ed.start)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", T("The range ends before it starts", "範囲の終わりが始まりより前です"));
		}
		else ImGui::TextUnformatted("");
		
		ImGui::Separator();
		ImGui::BeginDisabled(!valid);
		bool start=ImGui::Button(T("Start", "開始"));
		ImGui::EndDisabled();
		ImGui::SameLine();
		bool cancel=ImGui::Button(T("Cancel", "キャンセル")) || ImGui::IsKeyPressed(ImGuiKey_Escape);
		if(start)
		{
			recordRequest=RecordSettings();
			recordRequest.output=ed.path;
			recordRequest.startFrame=ed.start;
			recordRequest.endFrame=ed.end;
			recordRequest.width=ed.size[0];
			recordRequest.height=ed.size[1];
			recordRequest.fps=ed.fps;
			recordRequest.withMusic=ed.music;
			recordRequested=true;
			cancel=true;
		}
		if(cancel) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	
	//***Video output: progress***
	//the frame being written stays visible behind the progress dialog
	{
		static float dimAlpha=-1.0f;
		ImVec4 &dim=ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg];
		if(dimAlpha<0.0f) dimAlpha=dim.w;
		dim.w=recording ? 0.0f : dimAlpha;
	}
	if(recording && !ImGui::IsPopupOpen("###recording")) ImGui::OpenPopup("###recording");
	ImGui::SetNextWindowPos(ImVec2(centre.x, vpm->WorkPos.y+vpm->WorkSize.y-10.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
	if(ImGui::BeginPopupModal(T("Exporting video###recording", "動画出力中###recording"), NULL, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove))
	{
		if(!recording)
		{
			ImGui::CloseCurrentPopup();
		}
		else
		{
			string name=baseName(rec.output);
			ImGui::TextUnformatted(name.empty() ? rec.output.c_str() : name.c_str());
			if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s", rec.output.c_str());
			char overlay[64];
			float fraction;
			double elapsed=glfwGetTime()-recStartWall;
			if(recordPreroll)
			{
				int done=currentTime()/2;
				fraction=rec.startFrame>0 ? done/(float)rec.startFrame : 0.0f;
				snprintf(overlay, sizeof(overlay), "%d / %d", done, rec.startFrame);
				ImGui::Text("%s", T("Running the physics up to the first frame...", "最初のフレームまで物理演算を進めています…"));
			}
			else
			{
				fraction=recTotal>0 ? recFrame/(float)recTotal : 0.0f;
				snprintf(overlay, sizeof(overlay), "%d / %d", recFrame, recTotal);
				if(recFrame>0)
				{
					double remaining=elapsed/recFrame*(recTotal-recFrame);
					ImGui::Text(T("Frame %d (%d x %d, %d fps)   elapsed %d s, about %d s left", "フレーム %d（%d x %d、%d fps）  経過 %d 秒、残り約 %d 秒"),
					            rec.startFrame+recFrame*recStep/2, rec.width, rec.height, rec.fps, (int)elapsed, (int)(remaining+0.5));
				}
				else ImGui::Text(T("Frame %d (%d x %d, %d fps)", "フレーム %d（%d x %d、%d fps）"), rec.startFrame, rec.width, rec.height, rec.fps);
			}
			ImGui::ProgressBar(fraction, ImVec2(std::max(ImGui::CalcTextSize("0").x*50.0f, 360.0f), 0.0f), overlay);
			if(ImGui::Button(T("Stop", "中止")) || ImGui::IsKeyPressed(ImGuiKey_Escape)) recordCancel=true;
			if(recordCancel) { ImGui::SameLine(); ImGui::TextDisabled("%s", T("stopping...", "中止しています…")); }
		}
		ImGui::EndPopup();
	}
	
	//***Video output: result***
	if(!recordMessage.empty() && !recording && !ImGui::IsPopupOpen("###recorddone")) ImGui::OpenPopup("###recorddone");
	ImGui::SetNextWindowPos(centre, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if(ImGui::BeginPopupModal(T("Export video###recorddone", "動画出力###recorddone"), NULL, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings))
	{
		if(recordFailed) ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", T("Failed", "失敗しました"));
		else if(recordCancelled) ImGui::TextUnformatted(T("Stopped", "中止しました"));
		else ImGui::TextUnformatted(T("Done", "完了しました"));
		ImGui::TextUnformatted(recordMessage.c_str());
		if(ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			recordMessage.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	
	//***Loading / errors***
	if(!pendingLoads.empty())
	{
		ImGui::SetNextWindowPos(centre, 0, ImVec2(0.5f, 0.5f));
		if(ImGui::Begin("##loading", NULL, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoInputs))
		{
			ImGui::TextUnformatted(T("Loading...", "読み込み中…"));
			for(const PendingLoad &l : pendingLoads) if(!l.path.empty()) ImGui::TextDisabled("%s", baseName(l.path).c_str());
		}
		ImGui::End();
	}
	if(!loadError.empty() && !ImGui::IsPopupOpen("###loaderror"))
	{
		ImGui::OpenPopup("###loaderror");
	}
	ImGui::SetNextWindowPos(centre, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if(ImGui::BeginPopupModal(T("Error###loaderror", "エラー###loaderror"), NULL, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::TextUnformatted(loadError.c_str());
		if(ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			loadError.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	
	ImGui::Render();
	guiCapturesMouse=ImGui::GetIO().WantCaptureMouse;
	//typing in a text field or a dialog being open: no viewer shortcuts
	guiCapturesKeyboard=ImGui::GetIO().WantCaptureKeyboard || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
}

void Viewer::guiRender()
{
	if(!guiAvailable) return;
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	//restore the viewer's GL state (the backend restores most, not all)
	glUseProgram(shaderProgram);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
}

#else //!MMP_GUI

void Viewer::initGUI() {}
void Viewer::shutdownGUI() {}
void Viewer::guiFrame() {}
void Viewer::guiRender() {}

#endif
