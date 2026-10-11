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
		static const ImWchar extra[]={0x2190, 0x21FF, 0x25A0, 0x25FF, 0};
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
			if(ImGui::SliderInt("##timeline", &sliderFrame, 0, endFrame>0 ? endFrame : 1, fmt) && sliderFrame!=frame)
			{
				seekTo(sliderFrame*2);
			}
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

			//right-aligned: settings / hide
			const char *settingsLabel=T("Settings", "設定");
			const char *hideLabel=T("Hide (Tab)", "隠す (Tab)");
			float rightW=ImGui::CalcTextSize(settingsLabel).x+ImGui::CalcTextSize(hideLabel).x+style.FramePadding.x*4.0f+style.ItemSpacing.x;
			ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x-rightW));
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

	ImGui::Render();
	guiCapturesMouse=ImGui::GetIO().WantCaptureMouse;
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
