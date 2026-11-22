#pragma once

// What `spirula mesh` says while it runs.
//
// This is a child process of the GUI as well as a command of its own, and its
// output lands in the GUI's terminal panel, read by the same person who chose
// the interface language. So it is ours to translate, all of it -- the same
// rule `spirula sfm` follows in i18n/catalog/Sfm.h.
//
// Every line has the shape
//
//     [meshing] <stage>: <message>
//
// with all three parts translated. The GUI's progress bar reads that shape
// back (src/app/gui/MeshRunner.cpp): it compares the stage against the same
// `stage_*` messages the child printed with, and pulls counts out of the
// bodies with i18n::scan(), which matches a line against the message it came
// from rather than against English fragments. Translating both ends is
// therefore safe -- there is no English marker hiding in the protocol.
//
// Two consequences worth keeping in mind when editing:
//   - a `stage_*` string must stay a SHORT NOUN, not a sentence: it is a
//     column, and the line after it carries the detail;
//   - a body message's placeholders may be reordered freely, but the literal
//     text between them is what scan() matches on, so it must not be empty
//     between two adjacent placeholders. Keep a separator.
//
// The caption the progress bar shows under itself is a different string --
// `mesh_stage_*` in i18n/catalog/Log.h, a full phrase for a person reading a
// bar rather than a log.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace mesh {

// ===========================================================================
// The tag, and the two words that qualify a line
// ===========================================================================

SS_MSG(tag_meshing,
    EN("meshing"),
    JA("メッシュ化"),
    ZH_HANS("网格化"),
    ZH_HANT("網格化"),
    KO("메시 생성"),
    DE("Vernetzung"),
    FR("maillage"),
    ES("mallado"),
    PT("malha"),
    IT("mesh"),
    NL("mesh"),
    RU("построение меша"),
    TR("ağ oluşturma"));

SS_MSG(word_warning,
    EN("WARNING:"),
    JA("警告:"),
    ZH_HANS("警告："),
    ZH_HANT("警告："),
    KO("경고:"),
    DE("WARNUNG:"),
    FR("AVERTISSEMENT :"),
    ES("AVISO:"),
    PT("AVISO:"),
    IT("AVVISO:"),
    NL("WAARSCHUWING:"),
    RU("ПРЕДУПРЕЖДЕНИЕ:"),
    TR("UYARI:"));

SS_MSG(word_error,
    EN("error:"),
    JA("エラー:"),
    ZH_HANS("错误："),
    ZH_HANT("錯誤："),
    KO("오류:"),
    DE("Fehler:"),
    FR("erreur :"),
    ES("error:"),
    PT("erro:"),
    IT("errore:"),
    NL("fout:"),
    RU("ошибка:"),
    TR("hata:"));

// ===========================================================================
// Stages -- the column, in the order the pipeline reaches them
// ===========================================================================

SS_MSG(stage_loading,
    EN("loading"),
    JA("読み込み"),
    ZH_HANS("加载"),
    ZH_HANT("載入"),
    KO("불러오기"),
    DE("Laden"),
    FR("chargement"),
    ES("carga"),
    PT("carga"),
    IT("caricamento"),
    NL("laden"),
    RU("загрузка"),
    TR("yükleme"));

SS_MSG(stage_tsdf,
    EN("tsdf"),
    JA("TSDF"),
    ZH_HANS("TSDF"),
    ZH_HANT("TSDF"),
    KO("TSDF"),
    DE("TSDF"),
    FR("TSDF"),
    ES("TSDF"),
    PT("TSDF"),
    IT("TSDF"),
    NL("TSDF"),
    RU("TSDF"),
    TR("TSDF"));

SS_MSG(stage_point_cloud,
    EN("point cloud"),
    JA("点群"),
    ZH_HANS("点云"),
    ZH_HANT("點雲"),
    KO("점 구름"),
    DE("Punktwolke"),
    FR("nuage de points"),
    ES("nube de puntos"),
    PT("nuvem de pontos"),
    IT("nuvola di punti"),
    NL("puntenwolk"),
    RU("облако точек"),
    TR("nokta bulutu"));

// A person's name. It stays itself in every language -- it is what the
// literature and every other tool call this triangulation.
SS_MSG(stage_delaunay,
    EN("Delaunay"), JA("Delaunay"), ZH_HANS("Delaunay"), ZH_HANT("Delaunay"),
    KO("Delaunay"), DE("Delaunay"), FR("Delaunay"), ES("Delaunay"),
    PT("Delaunay"), IT("Delaunay"), NL("Delaunay"), RU("Делоне"),
    TR("Delaunay"));

SS_MSG(stage_occupancy,
    EN("occupancy"),
    JA("占有度"),
    ZH_HANS("占据场"),
    ZH_HANT("佔據場"),
    KO("점유도"),
    DE("Belegung"),
    FR("occupation"),
    ES("ocupación"),
    PT("ocupação"),
    IT("occupazione"),
    NL("bezetting"),
    RU("заполненность"),
    TR("doluluk"));

SS_MSG(stage_cut_edges,
    EN("cut edges"),
    JA("交差辺"),
    ZH_HANS("相交边"),
    ZH_HANT("相交邊"),
    KO("교차 모서리"),
    DE("Schnittkanten"),
    FR("arêtes coupées"),
    ES("aristas cortadas"),
    PT("arestas cortadas"),
    IT("spigoli tagliati"),
    NL("snijranden"),
    RU("рассечённые рёбра"),
    TR("kesişen kenarlar"));

SS_MSG(stage_bisection,
    EN("bisection"),
    JA("二分法"),
    ZH_HANS("二分"),
    ZH_HANT("二分"),
    KO("이분법"),
    DE("Bisektion"),
    FR("bissection"),
    ES("bisección"),
    PT("bisseção"),
    IT("bisezione"),
    NL("bisectie"),
    RU("деление пополам"),
    TR("ikiye bölme"));

SS_MSG(stage_marching,
    EN("marching tets"),
    JA("四面体マーチング"),
    ZH_HANS("四面体行进"),
    ZH_HANT("四面體行進"),
    KO("사면체 행진"),
    DE("Marching Tets"),
    FR("marching tets"),
    ES("marching tets"),
    PT("marching tets"),
    IT("marching tets"),
    NL("marching tets"),
    RU("марш тетраэдров"),
    TR("marching tets"));

SS_MSG(stage_merge,
    EN("merge"),
    JA("統合"),
    ZH_HANS("合并"),
    ZH_HANT("合併"),
    KO("병합"),
    DE("Zusammenführen"),
    FR("fusion"),
    ES("fusión"),
    PT("fusão"),
    IT("fusione"),
    NL("samenvoegen"),
    RU("слияние"),
    TR("birleştirme"));

SS_MSG(stage_cleanup,
    EN("cleanup"),
    JA("整理"),
    ZH_HANS("清理"),
    ZH_HANT("清理"),
    KO("정리"),
    DE("Aufräumen"),
    FR("nettoyage"),
    ES("limpieza"),
    PT("limpeza"),
    IT("pulizia"),
    NL("opschonen"),
    RU("очистка"),
    TR("temizlik"));

SS_MSG(stage_cull,
    EN("cull unseen"),
    JA("未観測の除去"),
    ZH_HANS("剔除未见"),
    ZH_HANT("剔除未見"),
    KO("미관측 제거"),
    DE("Unsichtbares weg"),
    FR("purge du non-vu"),
    ES("purga de lo no visto"),
    PT("purga do não visto"),
    IT("scarto del non visto"),
    NL("ongeziene weg"),
    RU("отсев невидимого"),
    TR("görülmeyeni ayıklama"));

SS_MSG(stage_quality,
    EN("quality"),
    JA("品質"),
    ZH_HANS("质量"),
    ZH_HANT("品質"),
    KO("품질"),
    DE("Qualität"),
    FR("qualité"),
    ES("calidad"),
    PT("qualidade"),
    IT("qualità"),
    NL("kwaliteit"),
    RU("качество"),
    TR("kalite"));

SS_MSG(stage_orient,
    EN("orient"),
    JA("向き"),
    ZH_HANS("定向"),
    ZH_HANT("定向"),
    KO("방향"),
    DE("Ausrichtung"),
    FR("orientation"),
    ES("orientación"),
    PT("orientação"),
    IT("orientamento"),
    NL("oriëntatie"),
    RU("ориентация"),
    TR("yönlendirme"));

SS_MSG(stage_color,
    EN("color"),
    JA("色"),
    ZH_HANS("颜色"),
    ZH_HANT("顏色"),
    KO("색"),
    DE("Farbe"),
    FR("couleur"),
    ES("color"),
    PT("cor"),
    IT("colore"),
    NL("kleur"),
    RU("цвет"),
    TR("renk"));

// The name of the coordinates, not a word: "UV" in every language.
SS_MSG(stage_uv,
    EN("UV"), JA("UV"), ZH_HANS("UV"), ZH_HANT("UV"), KO("UV"), DE("UV"),
    FR("UV"), ES("UV"), PT("UV"), IT("UV"), NL("UV"), RU("UV"), TR("UV"));

SS_MSG(stage_bake,
    EN("bake"),
    JA("焼き付け"),
    ZH_HANS("烘焙"),
    ZH_HANT("烘焙"),
    KO("굽기"),
    DE("Backen"),
    FR("cuisson"),
    ES("horneado"),
    PT("assamento"),
    IT("baking"),
    NL("bakken"),
    RU("запекание"),
    TR("pişirme"));

SS_MSG(stage_texture,
    EN("texture"),
    JA("テクスチャ"),
    ZH_HANS("纹理"),
    ZH_HANT("紋理"),
    KO("텍스처"),
    DE("Textur"),
    FR("texture"),
    ES("textura"),
    PT("textura"),
    IT("texture"),
    NL("textuur"),
    RU("текстура"),
    TR("doku"));

SS_MSG(stage_texel_density,
    EN("texel density"),
    JA("テクセル密度"),
    ZH_HANS("纹素密度"),
    ZH_HANT("紋素密度"),
    KO("텍셀 밀도"),
    DE("Texeldichte"),
    FR("densité de texels"),
    ES("densidad de téxeles"),
    PT("densidade de texels"),
    IT("densità di texel"),
    NL("texeldichtheid"),
    RU("плотность текселей"),
    TR("teksel yoğunluğu"));

SS_MSG(stage_stats,
    EN("stats"),
    JA("統計"),
    ZH_HANS("统计"),
    ZH_HANT("統計"),
    KO("통계"),
    DE("Statistik"),
    FR("statistiques"),
    ES("estadísticas"),
    PT("estatísticas"),
    IT("statistiche"),
    NL("statistiek"),
    RU("статистика"),
    TR("istatistik"));

SS_MSG(stage_wrote,
    EN("wrote"),
    JA("書き出し"),
    ZH_HANS("写出"),
    ZH_HANT("寫出"),
    KO("저장"),
    DE("geschrieben"),
    FR("écrit"),
    ES("escrito"),
    PT("escrito"),
    IT("scritto"),
    NL("geschreven"),
    RU("записано"),
    TR("yazıldı"));

SS_MSG(stage_done,
    EN("done"),
    JA("完了"),
    ZH_HANS("完成"),
    ZH_HANT("完成"),
    KO("완료"),
    DE("fertig"),
    FR("terminé"),
    ES("listo"),
    PT("pronto"),
    IT("fatto"),
    NL("klaar"),
    RU("готово"),
    TR("bitti"));

// Only reached through the debug hooks; the line under it is a dump of
// numbers rather than a sentence, and stays as it is.
SS_MSG(stage_debug,
    EN("debug"),
    JA("デバッグ"),
    ZH_HANS("调试"),
    ZH_HANT("除錯"),
    KO("디버그"),
    DE("Debug"),
    FR("débogage"),
    ES("depuración"),
    PT("depuração"),
    IT("debug"),
    NL("debug"),
    RU("отладка"),
    TR("hata ayıklama"));

// ===========================================================================
// Loading the model and its cameras
// ===========================================================================

SS_MSG(gaussians_loaded,
    EN("Gaussians: {0}"),
    JA("ガウシアン: {0}"),
    ZH_HANS("高斯球：{0}"),
    ZH_HANT("高斯球：{0}"),
    KO("가우시안: {0}"),
    DE("Gaußfunktionen: {0}"),
    FR("gaussiennes : {0}"),
    ES("gaussianas: {0}"),
    PT("gaussianas: {0}"),
    IT("gaussiane: {0}"),
    NL("gaussianen: {0}"),
    RU("гауссианы: {0}"),
    TR("gaussian: {0}"));

SS_MSG(reading_cameras,
    EN("reading the cameras from {0}"),
    JA("{0} からカメラを読み込みます"),
    ZH_HANS("正在从 {0} 读取相机"),
    ZH_HANT("正在從 {0} 讀取相機"),
    KO("{0} 에서 카메라를 읽습니다"),
    DE("Kameras werden aus {0} gelesen"),
    FR("lecture des caméras dans {0}"),
    ES("leyendo las cámaras de {0}"),
    PT("lendo as câmeras de {0}"),
    IT("lettura delle camere da {0}"),
    NL("camera's lezen uit {0}"),
    RU("чтение камер из {0}"),
    TR("kameralar {0} konumundan okunuyor"));

SS_MSG(cameras_loaded,
    EN("cameras: {0} ({1}x{2}, {3})"),
    JA("カメラ: {0}（{1}x{2}、{3}）"),
    ZH_HANS("相机：{0}（{1}x{2}，{3}）"),
    ZH_HANT("相機：{0}（{1}x{2}，{3}）"),
    KO("카메라: {0}({1}x{2}, {3})"),
    DE("Kameras: {0} ({1}x{2}, {3})"),
    FR("caméras : {0} ({1}x{2}, {3})"),
    ES("cámaras: {0} ({1}x{2}, {3})"),
    PT("câmeras: {0} ({1}x{2}, {3})"),
    IT("camere: {0} ({1}x{2}, {3})"),
    NL("camera's: {0} ({1}x{2}, {3})"),
    RU("камеры: {0} ({1}x{2}, {3})"),
    TR("kamera: {0} ({1}x{2}, {3})"));

SS_MSG(gaussians_kept,
    EN("Gaussians kept: {0}/{1}, points: {2}, relative scale: {3}"),
    JA("残したガウシアン: {0}/{1}、点: {2}、相対スケール: {3}"),
    ZH_HANS("保留的高斯球：{0}/{1}，点：{2}，相对尺度：{3}"),
    ZH_HANT("保留的高斯球：{0}/{1}，點：{2}，相對尺度：{3}"),
    KO("남긴 가우시안: {0}/{1}, 점: {2}, 상대 크기: {3}"),
    DE("behaltene Gaußfunktionen: {0}/{1}, Punkte: {2}, relative Skala: {3}"),
    FR("gaussiennes conservées : {0}/{1}, points : {2}, échelle relative : {3}"),
    ES("gaussianas conservadas: {0}/{1}, puntos: {2}, escala relativa: {3}"),
    PT("gaussianas mantidas: {0}/{1}, pontos: {2}, escala relativa: {3}"),
    IT("gaussiane tenute: {0}/{1}, punti: {2}, scala relativa: {3}"),
    NL("behouden gaussianen: {0}/{1}, punten: {2}, relatieve schaal: {3}"),
    RU("оставлено гауссиан: {0}/{1}, точки: {2}, относительный масштаб: {3}"),
    TR("tutulan gaussian: {0}/{1}, nokta: {2}, göreli ölçek: {3}"));

SS_MSG(cameras_selected,
    EN("cameras used: {0}/{1} (k-means medoids)"),
    JA("使うカメラ: {0}/{1}（k-means のメドイド）"),
    ZH_HANS("使用的相机：{0}/{1}（k-means 中心点）"),
    ZH_HANT("使用的相機：{0}/{1}（k-means 中心點）"),
    KO("사용할 카메라: {0}/{1}(k-평균 메도이드)"),
    DE("verwendete Kameras: {0}/{1} (k-Means-Medoide)"),
    FR("caméras utilisées : {0}/{1} (médoïdes k-means)"),
    ES("cámaras usadas: {0}/{1} (medoides de k-means)"),
    PT("câmeras usadas: {0}/{1} (medoides de k-means)"),
    IT("camere usate: {0}/{1} (medoidi k-means)"),
    NL("gebruikte camera's: {0}/{1} (k-means-medoïden)"),
    RU("используются камеры: {0}/{1} (медоиды k-means)"),
    TR("kullanılan kamera: {0}/{1} (k-ortalama medoidleri)"));

SS_MSG(intrinsics_uniform,
    EN("camera intrinsics: {0}x{1}, model {2} -- the render path is ready"),
    JA("カメラ内部パラメータ: {0}x{1}、モデル {2} -- レンダリング経路の準備ができました"),
    ZH_HANS("相机内参：{0}x{1}，模型 {2} —— 渲染路径已就绪"),
    ZH_HANT("相機內參：{0}x{1}，模型 {2} —— 算繪路徑已就緒"),
    KO("카메라 내부 파라미터: {0}x{1}, 모델 {2} -- 렌더링 경로가 준비되었습니다"),
    DE("Kamera-Intrinsics: {0}x{1}, Modell {2} -- der Renderpfad steht bereit"),
    FR("paramètres internes : {0}x{1}, modèle {2} -- le chemin de rendu est prêt"),
    ES("parámetros internos: {0}x{1}, modelo {2}: la ruta de renderizado está lista"),
    PT("parâmetros internos: {0}x{1}, modelo {2} -- o caminho de renderização está pronto"),
    IT("parametri interni: {0}x{1}, modello {2} -- il percorso di rendering è pronto"),
    NL("camera-intrinsieken: {0}x{1}, model {2} -- het renderpad staat klaar"),
    RU("внутренние параметры камеры: {0}x{1}, модель {2} -- путь отрисовки готов"),
    TR("kamera iç parametreleri: {0}x{1}, model {2} -- işleme yolu hazır"));

SS_MSG(intrinsics_varied,
    EN("camera intrinsics: a resolution per camera, model {0} -- the render "
       "path is ready"),
    JA("カメラ内部パラメータ: 解像度はカメラごと、モデル {0} -- レンダリング経路の準備ができました"),
    ZH_HANS("相机内参：分辨率逐相机不同，模型 {0} —— 渲染路径已就绪"),
    ZH_HANT("相機內參：解析度逐相機不同，模型 {0} —— 算繪路徑已就緒"),
    KO("카메라 내부 파라미터: 해상도는 카메라마다 다름, 모델 {0} -- 렌더링 경로가 준비되었습니다"),
    DE("Kamera-Intrinsics: eine Auflösung je Kamera, Modell {0} -- der "
       "Renderpfad steht bereit"),
    FR("paramètres internes : une résolution par caméra, modèle {0} -- le "
       "chemin de rendu est prêt"),
    ES("parámetros internos: una resolución por cámara, modelo {0}: la ruta de "
       "renderizado está lista"),
    PT("parâmetros internos: uma resolução por câmera, modelo {0} -- o caminho "
       "de renderização está pronto"),
    IT("parametri interni: una risoluzione per camera, modello {0} -- il "
       "percorso di rendering è pronto"),
    NL("camera-intrinsieken: een resolutie per camera, model {0} -- het "
       "renderpad staat klaar"),
    RU("внутренние параметры камеры: своё разрешение у каждой, модель {0} -- "
       "путь отрисовки готов"),
    TR("kamera iç parametreleri: her kamerada ayrı çözünürlük, model {0} -- "
       "işleme yolu hazır"));

// Read back by the GUI to move the bar WITHIN a stage, so the two numbers must
// stay separated by something -- see the note at the top of this file.
SS_MSG(cameras_rendered,
    EN("cameras rendered: {0}/{1}"),
    JA("描画したカメラ: {0}/{1}"),
    ZH_HANS("已渲染相机：{0}/{1}"),
    ZH_HANT("已算繪相機：{0}/{1}"),
    KO("렌더링한 카메라: {0}/{1}"),
    DE("gerenderte Kameras: {0}/{1}"),
    FR("caméras rendues : {0}/{1}"),
    ES("cámaras renderizadas: {0}/{1}"),
    PT("câmeras renderizadas: {0}/{1}"),
    IT("camere renderizzate: {0}/{1}"),
    NL("gerenderde camera's: {0}/{1}"),
    RU("отрисовано камер: {0}/{1}"),
    TR("işlenen kamera: {0}/{1}"));

// ===========================================================================
// The pipeline
// ===========================================================================

// A stage that only reports how long it took. The seconds are formatted by the
// caller, so this is a unit and nothing else.
SS_MSG(seconds,
    EN("{0}s"),
    JA("{0}秒"),
    ZH_HANS("{0} 秒"),
    ZH_HANT("{0} 秒"),
    KO("{0}초"),
    DE("{0} s"),
    FR("{0} s"),
    ES("{0} s"),
    PT("{0} s"),
    IT("{0} s"),
    NL("{0} s"),
    RU("{0} с"),
    TR("{0} sn"));

SS_MSG(point_cloud_sampling,
    EN("sampling points from the Gaussians (total: {0})..."),
    JA("ガウシアンから点をサンプリングしています（総数: {0}）..."),
    ZH_HANS("正在从高斯球采样点（总数：{0}）……"),
    ZH_HANT("正在從高斯球取樣點（總數：{0}）……"),
    KO("가우시안에서 점을 뽑는 중(전체: {0})..."),
    DE("Punkte werden aus den Gaußfunktionen abgetastet (insgesamt: {0}) ..."),
    FR("échantillonnage de points depuis les gaussiennes (total : {0})..."),
    ES("muestreando puntos de las gaussianas (total: {0})..."),
    PT("amostrando pontos das gaussianas (total: {0})..."),
    IT("campionamento di punti dalle gaussiane (totale: {0})..."),
    NL("punten bemonsteren uit de gaussianen (totaal: {0})..."),
    RU("выборка точек из гауссиан (всего: {0})..."),
    TR("gaussian'lardan nokta örnekleniyor (toplam: {0})..."));

SS_MSG(point_cloud_done,
    EN("points: {0} ({1}s)"),
    JA("点: {0}（{1}秒）"),
    ZH_HANS("点：{0}（{1} 秒）"),
    ZH_HANT("點：{0}（{1} 秒）"),
    KO("점: {0}({1}초)"),
    DE("Punkte: {0} ({1} s)"),
    FR("points : {0} ({1} s)"),
    ES("puntos: {0} ({1} s)"),
    PT("pontos: {0} ({1} s)"),
    IT("punti: {0} ({1} s)"),
    NL("punten: {0} ({1} s)"),
    RU("точки: {0} ({1} с)"),
    TR("nokta: {0} ({1} sn)"));

SS_MSG(delaunay_start,
    EN("triangulating the points (total: {0})..."),
    JA("点を三角形分割しています（総数: {0}）..."),
    ZH_HANS("正在对点做三角剖分（总数：{0}）……"),
    ZH_HANT("正在對點做三角剖分（總數：{0}）……"),
    KO("점을 삼각 분할하는 중(전체: {0})..."),
    DE("Punkte werden trianguliert (insgesamt: {0}) ..."),
    FR("triangulation des points (total : {0})..."),
    ES("triangulando los puntos (total: {0})..."),
    PT("triangulando os pontos (total: {0})..."),
    IT("triangolazione dei punti (totale: {0})..."),
    NL("punten trianguleren (totaal: {0})..."),
    RU("триангуляция точек (всего: {0})..."),
    TR("noktalar üçgenleniyor (toplam: {0})..."));

SS_MSG(delaunay_done,
    EN("tetrahedra: {0}, threads: {1} ({2}s)"),
    JA("四面体: {0}、スレッド: {1}（{2}秒）"),
    ZH_HANS("四面体：{0}，线程：{1}（{2} 秒）"),
    ZH_HANT("四面體：{0}，執行緒：{1}（{2} 秒）"),
    KO("사면체: {0}, 스레드: {1}({2}초)"),
    DE("Tetraeder: {0}, Threads: {1} ({2} s)"),
    FR("tétraèdres : {0}, threads : {1} ({2} s)"),
    ES("tetraedros: {0}, hilos: {1} ({2} s)"),
    PT("tetraedros: {0}, threads: {1} ({2} s)"),
    IT("tetraedri: {0}, thread: {1} ({2} s)"),
    NL("tetraëders: {0}, threads: {1} ({2} s)"),
    RU("тетраэдры: {0}, потоки: {1} ({2} с)"),
    TR("dörtyüzlü: {0}, iş parçacığı: {1} ({2} sn)"));

SS_MSG(no_tetrahedra,
    EN("no tetrahedra came out of the triangulation"),
    JA("三角形分割から四面体が得られませんでした"),
    ZH_HANS("三角剖分没有产生任何四面体"),
    ZH_HANT("三角剖分沒有產生任何四面體"),
    KO("삼각 분할에서 사면체가 나오지 않았습니다"),
    DE("aus der Triangulierung kamen keine Tetraeder"),
    FR("la triangulation n'a produit aucun tétraèdre"),
    ES("la triangulación no produjo ningún tetraedro"),
    PT("a triangulação não produziu nenhum tetraedro"),
    IT("la triangolazione non ha prodotto tetraedri"),
    NL("de triangulatie leverde geen tetraëders op"),
    RU("триангуляция не дала ни одного тетраэдра"),
    TR("üçgenlemeden hiç dörtyüzlü çıkmadı"));

SS_MSG(too_many_gaussians,
    EN("{0} Gaussians exceed meshing limits: {1} samples = {2} Gaussians at {3} samples each. Retrain with lower --cap-max."),
    JA("ガウス数 {0} はメッシュ化の上限超過：{1} サンプル＝各{3}点で {2} 個。--cap-max を下げて再学習してください。"),
    ZH_HANS("{0} 个高斯超过网格化上限：{1} 个样本＝每个 {3} 点时 {2} 个高斯。降低 --cap-max 后重训。"),
    ZH_HANT("{0} 個高斯超過網格化上限：{1} 個樣本＝每個 {3} 點時 {2} 個高斯。降低 --cap-max 後重訓。"),
    KO("가우스 {0}개는 메시 한도 초과: 샘플 {1}개 = 각 {3}점씩 가우스 {2}개. --cap-max를 낮춰 재학습하세요."),
    DE("{0} Gauß-Verteilungen überschreiten das Mesh-Limit: {1} Samples = {2} bei je {3} Punkten. Mit kleinerem --cap-max neu trainieren."),
    FR("{0} gaussiennes dépassent la limite : {1} échantillons = {2} à {3} points chacune. Réentraîner avec --cap-max réduit."),
    ES("{0} gaussianas superan el límite: {1} muestras = {2} a {3} puntos cada una. Reentrena con menor --cap-max."),
    PT("{0} gaussianas excedem o limite: {1} amostras = {2} a {3} pontos cada. Treine de novo com --cap-max menor."),
    IT("{0} gaussiane superano il limite: {1} campioni = {2} con {3} punti ciascuna. Riaddestra con --cap-max inferiore."),
    NL("{0} Gaussians overschrijden de grens: {1} samples = {2} bij {3} punten elk. Train opnieuw met lagere --cap-max."),
    RU("{0} гауссиан превышают предел: {1} отсчётов = {2} по {3} точек на каждую. Переобучите с меньшим --cap-max."),
    TR("{0} Gauss sınırı aşıyor: {1} örnek = her biri {3} noktadan {2} Gauss. Daha düşük --cap-max ile yeniden eğitin."));

SS_MSG(occupancy_start,
    EN("evaluating the points (total: {0})..."),
    JA("点を評価しています（総数: {0}）..."),
    ZH_HANS("正在计算点的取值（总数：{0}）……"),
    ZH_HANT("正在計算點的取值（總數：{0}）……"),
    KO("점을 계산하는 중(전체: {0})..."),
    DE("Punkte werden ausgewertet (insgesamt: {0}) ..."),
    FR("évaluation des points (total : {0})..."),
    ES("evaluando los puntos (total: {0})..."),
    PT("avaliando os pontos (total: {0})..."),
    IT("valutazione dei punti (totale: {0})..."),
    NL("punten evalueren (totaal: {0})..."),
    RU("вычисление в точках (всего: {0})..."),
    TR("noktalar hesaplanıyor (toplam: {0})..."));

SS_MSG(cut_edges_start,
    EN("scanning the tetrahedra (total: {0})..."),
    JA("四面体を走査しています（総数: {0}）..."),
    ZH_HANS("正在扫描四面体（总数：{0}）……"),
    ZH_HANT("正在掃描四面體（總數：{0}）……"),
    KO("사면체를 훑는 중(전체: {0})..."),
    DE("Tetraeder werden durchgesehen (insgesamt: {0}) ..."),
    FR("balayage des tétraèdres (total : {0})..."),
    ES("recorriendo los tetraedros (total: {0})..."),
    PT("percorrendo os tetraedros (total: {0})..."),
    IT("scansione dei tetraedri (totale: {0})..."),
    NL("tetraëders doorlopen (totaal: {0})..."),
    RU("просмотр тетраэдров (всего: {0})..."),
    TR("dörtyüzlüler taranıyor (toplam: {0})..."));

SS_MSG(pockets_sealed,
    EN("free vertices no camera connects to, made solid: {0} ({1}s)"),
    JA("どのカメラにもつながらない空き頂点を充填: {0}（{1}秒）"),
    ZH_HANS("与任何相机都不连通的空顶点，已填实：{0}（{1} 秒）"),
    ZH_HANT("與任何相機都不連通的空頂點，已填實：{0}（{1} 秒）"),
    KO("어느 카메라와도 이어지지 않아 채운 빈 정점: {0}({1}초)"),
    DE("freie Ecken ohne Verbindung zu einer Kamera, gefüllt: {0} ({1} s)"),
    FR("sommets libres reliés à aucune caméra, remplis : {0} ({1} s)"),
    ES("vértices libres sin conexión con ninguna cámara, rellenados: {0} ({1} s)"),
    PT("vértices livres sem ligação a nenhuma câmera, preenchidos: {0} ({1} s)"),
    IT("vertici liberi non collegati ad alcuna camera, riempiti: {0} ({1} s)"),
    NL("vrije hoekpunten zonder verbinding met een camera, gevuld: {0} ({1} s)"),
    RU("свободные вершины без связи с камерами, заполнено: {0} ({1} с)"),
    TR("hiçbir kameraya bağlanmayan boş köşeler dolduruldu: {0} ({1} sn)"));

SS_MSG(cut_edges_done,
    EN("edges: {0} ({1}s)"),
    JA("辺: {0}（{1}秒）"),
    ZH_HANS("边：{0}（{1} 秒）"),
    ZH_HANT("邊：{0}（{1} 秒）"),
    KO("모서리: {0}({1}초)"),
    DE("Kanten: {0} ({1} s)"),
    FR("arêtes : {0} ({1} s)"),
    ES("aristas: {0} ({1} s)"),
    PT("arestas: {0} ({1} s)"),
    IT("spigoli: {0} ({1} s)"),
    NL("randen: {0} ({1} s)"),
    RU("рёбра: {0} ({1} с)"),
    TR("kenar: {0} ({1} sn)"));

SS_MSG(isosurface_empty,
    EN("the isosurface is empty -- no edge crosses {0}"),
    JA("等値面が空です -- {0} をまたぐ辺がありません"),
    ZH_HANS("等值面为空 —— 没有边跨过 {0}"),
    ZH_HANT("等值面為空 —— 沒有邊跨過 {0}"),
    KO("등가면이 비어 있습니다 -- {0} 을(를) 가로지르는 모서리가 없습니다"),
    DE("die Isofläche ist leer -- keine Kante kreuzt {0}"),
    FR("l'isosurface est vide -- aucune arête ne traverse {0}"),
    ES("la isosuperficie está vacía: ninguna arista cruza {0}"),
    PT("a isossuperfície está vazia -- nenhuma aresta cruza {0}"),
    IT("l'isosuperficie è vuota -- nessuno spigolo attraversa {0}"),
    NL("het isovlak is leeg -- geen enkele rand kruist {0}"),
    RU("изоповерхность пуста -- ни одно ребро не пересекает {0}"),
    TR("eş yüzey boş -- {0} değerini geçen kenar yok"));

SS_MSG(bisection_start,
    EN("edges: {0}, iterations: {1}..."),
    JA("辺: {0}、反復: {1}..."),
    ZH_HANS("边：{0}，迭代：{1}……"),
    ZH_HANT("邊：{0}，疊代：{1}……"),
    KO("모서리: {0}, 반복: {1}..."),
    DE("Kanten: {0}, Iterationen: {1} ..."),
    FR("arêtes : {0}, itérations : {1}..."),
    ES("aristas: {0}, iteraciones: {1}..."),
    PT("arestas: {0}, iterações: {1}..."),
    IT("spigoli: {0}, iterazioni: {1}..."),
    NL("randen: {0}, iteraties: {1}..."),
    RU("рёбра: {0}, итерации: {1}..."),
    TR("kenar: {0}, yineleme: {1}..."));

SS_MSG(marching_done,
    EN("raw faces: {0} ({1}s)"),
    JA("生の面: {0}（{1}秒）"),
    ZH_HANS("原始面：{0}（{1} 秒）"),
    ZH_HANT("原始面：{0}（{1} 秒）"),
    KO("가공 전 면: {0}({1}초)"),
    DE("rohe Flächen: {0} ({1} s)"),
    FR("faces brutes : {0} ({1} s)"),
    ES("caras en bruto: {0} ({1} s)"),
    PT("faces brutas: {0} ({1} s)"),
    IT("facce grezze: {0} ({1} s)"),
    NL("ruwe vlakken: {0} ({1} s)"),
    RU("необработанные грани: {0} ({1} с)"),
    TR("ham yüz: {0} ({1} sn)"));

SS_MSG(tsdf_start,
    EN("fusing {0} cameras' depth -- voxel {1}, truncation {2}"),
    JA("{0} 台のカメラの深度を統合します -- ボクセル {1}、打ち切り {2}"),
    ZH_HANS("融合 {0} 台相机的深度 -- 体素 {1}，截断 {2}"),
    ZH_HANT("融合 {0} 台相機的深度 -- 體素 {1}，截斷 {2}"),
    KO("카메라 {0}대의 깊이를 융합합니다 -- 복셀 {1}, 절단 {2}"),
    DE("Tiefe von {0} Kameras wird fusioniert -- Voxel {1}, Kappung {2}"),
    FR("fusion de la profondeur de {0} caméras -- voxel {1}, troncature {2}"),
    ES("fusionando la profundidad de {0} cámaras -- vóxel {1}, truncamiento {2}"),
    PT("fundindo a profundidade de {0} câmeras -- voxel {1}, truncamento {2}"),
    IT("fusione della profondità di {0} camere -- voxel {1}, troncamento {2}"),
    NL("diepte van {0} camera's samenvoegen -- voxel {1}, afkapping {2}"),
    RU("слияние глубины {0} камер -- воксель {1}, усечение {2}"),
    TR("{0} kameranın derinliği birleştiriliyor -- voksel {1}, kesme {2}"));

SS_MSG(tsdf_fused,
    EN("blocks: {0} ({1} MB, {2}s)"),
    JA("ブロック: {0}（{1} MB、{2}秒）"),
    ZH_HANS("块：{0}（{1} MB，{2} 秒）"),
    ZH_HANT("區塊：{0}（{1} MB，{2} 秒）"),
    KO("블록: {0}({1} MB, {2}초)"),
    DE("Blöcke: {0} ({1} MB, {2} s)"),
    FR("blocs : {0} ({1} Mo, {2} s)"),
    ES("bloques: {0} ({1} MB, {2} s)"),
    PT("blocos: {0} ({1} MB, {2} s)"),
    IT("blocchi: {0} ({1} MB, {2} s)"),
    NL("blokken: {0} ({1} MB, {2} s)"),
    RU("блоки: {0} ({1} МБ, {2} с)"),
    TR("blok: {0} ({1} MB, {2} sn)"));

SS_MSG(tsdf_empty,
    EN("the fused depth has no surface: no camera saw anything opaque"),
    JA("統合した深度に面がありません。どのカメラにも不透明なものが写っていません"),
    ZH_HANS("融合后的深度中没有表面：没有相机看到不透明的内容"),
    ZH_HANT("融合後的深度中沒有表面：沒有相機看到不透明的內容"),
    KO("융합한 깊이에 표면이 없습니다. 어느 카메라에도 불투명한 것이 보이지 않습니다"),
    DE("Die fusionierte Tiefe hat keine Oberfläche: keine Kamera sah etwas Deckendes"),
    FR("la profondeur fusionnée n'a aucune surface : aucune caméra n'a rien vu d'opaque"),
    ES("la profundidad fusionada no tiene superficie: ninguna cámara vio nada opaco"),
    PT("a profundidade fundida não tem superfície: nenhuma câmera viu nada opaco"),
    IT("la profondità fusa non ha superfici: nessuna camera ha visto nulla di opaco"),
    NL("de samengevoegde diepte heeft geen oppervlak: geen camera zag iets ondoorzichtigs"),
    RU("в слитой глубине нет поверхности: ни одна камера не видела ничего непрозрачного"),
    TR("birleştirilen derinlikte yüzey yok: hiçbir kamera opak bir şey görmedi"));

SS_MSG(tsdf_needs_cameras,
    EN("--method tsdf fuses camera depth and needs the dataset's cameras: pass --data <dataset_dir>"),
    JA("--method tsdf はカメラの深度を統合するので、データセットのカメラが必要です。--data <dataset_dir> を渡してください"),
    ZH_HANS("--method tsdf 要融合相机深度，需要数据集的相机：请加上 --data <dataset_dir>"),
    ZH_HANT("--method tsdf 要融合相機深度，需要資料集的相機：請加上 --data <dataset_dir>"),
    KO("--method tsdf는 카메라 깊이를 융합하므로 데이터셋의 카메라가 필요합니다. --data <dataset_dir>를 주세요"),
    DE("--method tsdf fusioniert Kameratiefe und braucht die Kameras des Datensatzes: --data <dataset_dir> angeben"),
    FR("--method tsdf fusionne la profondeur des caméras et a besoin des caméras du jeu de données : passez --data <dataset_dir>"),
    ES("--method tsdf fusiona la profundidad de las cámaras y necesita las cámaras del conjunto de datos: pasa --data <dataset_dir>"),
    PT("--method tsdf funde a profundidade das câmeras e precisa das câmeras do conjunto de dados: passe --data <dataset_dir>"),
    IT("--method tsdf fonde la profondità delle camere e richiede le camere del dataset: passa --data <dataset_dir>"),
    NL("--method tsdf voegt cameradiepte samen en heeft de camera's van de dataset nodig: geef --data <dataset_dir> op"),
    RU("--method tsdf сливает глубину камер и требует камеры набора данных: укажите --data <dataset_dir>"),
    TR("--method tsdf kamera derinliğini birleştirir ve veri kümesinin kameralarına ihtiyaç duyar: --data <dataset_dir> verin"));

SS_MSG(merge_result,
    EN("collapses: {0} -- vertices: {1}, faces: {2}"),
    JA("縮約: {0} -- 頂点: {1}、面: {2}"),
    ZH_HANS("坍缩：{0} —— 顶点：{1}，面：{2}"),
    ZH_HANT("塌縮：{0} —— 頂點：{1}，面：{2}"),
    KO("축약: {0} -- 정점: {1}, 면: {2}"),
    DE("Kollapse: {0} -- Ecken: {1}, Flächen: {2}"),
    FR("effondrements : {0} -- sommets : {1}, faces : {2}"),
    ES("colapsos: {0}: vértices: {1}, caras: {2}"),
    PT("colapsos: {0} -- vértices: {1}, faces: {2}"),
    IT("collassi: {0} -- vertici: {1}, facce: {2}"),
    NL("inklappingen: {0} -- hoekpunten: {1}, vlakken: {2}"),
    RU("схлопываний: {0} -- вершины: {1}, грани: {2}"),
    TR("çökertme: {0} -- köşe: {1}, yüz: {2}"));

SS_MSG(cull_result,
    EN("vertices dropped: {0}/{1}, faces dropped: {2} -- vertices: {3}, "
       "faces: {4} ({5}s)"),
    JA("落とした頂点: {0}/{1}、落とした面: {2} -- 頂点: {3}、面: {4}（{5}秒）"),
    ZH_HANS("丢弃的顶点：{0}/{1}，丢弃的面：{2} —— 顶点：{3}，面：{4}（{5} 秒）"),
    ZH_HANT("丟棄的頂點：{0}/{1}，丟棄的面：{2} —— 頂點：{3}，面：{4}（{5} 秒）"),
    KO("버린 정점: {0}/{1}, 버린 면: {2} -- 정점: {3}, 면: {4}({5}초)"),
    DE("verworfene Ecken: {0}/{1}, verworfene Flächen: {2} -- Ecken: {3}, "
       "Flächen: {4} ({5} s)"),
    FR("sommets écartés : {0}/{1}, faces écartées : {2} -- sommets : {3}, "
       "faces : {4} ({5} s)"),
    ES("vértices descartados: {0}/{1}, caras descartadas: {2}: vértices: {3}, "
       "caras: {4} ({5} s)"),
    PT("vértices descartados: {0}/{1}, faces descartadas: {2} -- vértices: {3}, "
       "faces: {4} ({5} s)"),
    IT("vertici scartati: {0}/{1}, facce scartate: {2} -- vertici: {3}, "
       "facce: {4} ({5} s)"),
    NL("weggelaten hoekpunten: {0}/{1}, weggelaten vlakken: {2} -- "
       "hoekpunten: {3}, vlakken: {4} ({5} s)"),
    RU("отброшено вершин: {0}/{1}, отброшено граней: {2} -- вершины: {3}, "
       "грани: {4} ({5} с)"),
    TR("atılan köşe: {0}/{1}, atılan yüz: {2} -- köşe: {3}, yüz: {4} ({5} sn)"));

// ===========================================================================
// Cleanup -- the passes that repair the raw surface
// ===========================================================================

SS_MSG(dedup_faces,
    EN("duplicate faces removed: {0} -- faces: {1}"),
    JA("取り除いた重複面: {0} -- 面: {1}"),
    ZH_HANS("移除的重复面：{0} —— 面：{1}"),
    ZH_HANT("移除的重複面：{0} —— 面：{1}"),
    KO("없앤 중복 면: {0} -- 면: {1}"),
    DE("entfernte doppelte Flächen: {0} -- Flächen: {1}"),
    FR("faces en double supprimées : {0} -- faces : {1}"),
    ES("caras duplicadas eliminadas: {0}: caras: {1}"),
    PT("faces duplicadas removidas: {0} -- faces: {1}"),
    IT("facce duplicate rimosse: {0} -- facce: {1}"),
    NL("verwijderde dubbele vlakken: {0} -- vlakken: {1}"),
    RU("удалено дублирующих граней: {0} -- грани: {1}"),
    TR("kaldırılan yinelenen yüz: {0} -- yüz: {1}"));

SS_MSG(floaters_removed,
    EN("floaters -- faces removed: {0}, vertices removed: {1}; vertices: {2}, "
       "faces: {3}"),
    JA("浮遊片 -- 取り除いた面: {0}、取り除いた頂点: {1}、頂点: {2}、面: {3}"),
    ZH_HANS("浮块 —— 移除的面：{0}，移除的顶点：{1}；顶点：{2}，面：{3}"),
    ZH_HANT("浮塊 —— 移除的面：{0}，移除的頂點：{1}；頂點：{2}，面：{3}"),
    KO("떠 있는 조각 -- 없앤 면: {0}, 없앤 정점: {1}; 정점: {2}, 면: {3}"),
    DE("Schwebeteile -- entfernte Flächen: {0}, entfernte Ecken: {1}; "
       "Ecken: {2}, Flächen: {3}"),
    FR("morceaux flottants -- faces supprimées : {0}, sommets supprimés : {1} ; "
       "sommets : {2}, faces : {3}"),
    ES("trozos sueltos: caras eliminadas: {0}, vértices eliminados: {1}; "
       "vértices: {2}, caras: {3}"),
    PT("pedaços soltos -- faces removidas: {0}, vértices removidos: {1}; "
       "vértices: {2}, faces: {3}"),
    IT("pezzi fluttuanti -- facce rimosse: {0}, vertici rimossi: {1}; "
       "vertici: {2}, facce: {3}"),
    NL("zwevende stukken -- verwijderde vlakken: {0}, verwijderde "
       "hoekpunten: {1}; hoekpunten: {2}, vlakken: {3}"),
    RU("плавающие куски -- удалено граней: {0}, удалено вершин: {1}; "
       "вершины: {2}, грани: {3}"),
    TR("yüzen parçalar -- kaldırılan yüz: {0}, kaldırılan köşe: {1}; köşe: {2}, "
       "yüz: {3}"));

SS_MSG(defects_removed,
    EN("faces cut out (crossing, sliver, folded): {0} -- vertices: {1}, faces: {2}"),
    JA("切り取った面（交差・細長・折れ）: {0} -- 頂点: {1}、面: {2}"),
    ZH_HANS("切除的面（相交、细长、折叠）：{0} —— 顶点：{1}，面：{2}"),
    ZH_HANT("切除的面（相交、細長、摺疊）：{0} —— 頂點：{1}，面：{2}"),
    KO("잘라 낸 면(교차, 가늘음, 접힘): {0} -- 정점: {1}, 면: {2}"),
    DE("herausgeschnittene Flächen (schneidend, spitz, gefaltet): {0} -- Ecken: {1}, Flächen: {2}"),
    FR("faces découpées (croisées, effilées, repliées) : {0} -- sommets : {1}, faces : {2}"),
    ES("caras recortadas (cruzadas, astilla, plegadas): {0} -- vértices: {1}, caras: {2}"),
    PT("faces cortadas (cruzadas, lasca, dobradas): {0} -- vértices: {1}, faces: {2}"),
    IT("facce ritagliate (incrociate, scheggia, ripiegate): {0} -- vertici: {1}, facce: {2}"),
    NL("uitgesneden vlakken (snijdend, splinter, gevouwen): {0} -- hoekpunten: {1}, vlakken: {2}"),
    RU("вырезано граней (пересекающих, игольчатых, сложенных): {0} -- вершины: {1}, грани: {2}"),
    TR("kesilip atılan yüzler (kesişen, kıymık, katlanmış): {0} -- köşe: {1}, yüz: {2}"));

SS_MSG(holes_filled,
    EN("holes filled: {0} (smaller than {1} of their component, or at most {2} "
       "edges), faces added: {3} -- vertices: {4}, faces: {5}"),
    JA("埋めた穴: {0}（連結成分の {1} 未満、または辺 {2} 以下）、加えた面: {3} "
       "-- 頂点: {4}、面: {5}"),
    ZH_HANS("填补的洞：{0}（小于所在连通块的 {1}，或至多 {2} 条边），新增的面：{3} "
            "—— 顶点：{4}，面：{5}"),
    ZH_HANT("填補的洞：{0}（小於所在連通塊的 {1}，或至多 {2} 條邊），新增的面：{3} "
            "—— 頂點：{4}，面：{5}"),
    KO("메운 구멍: {0}(연결 성분의 {1} 미만이거나 모서리 {2}개 이하), 더한 면: {3} "
       "-- 정점: {4}, 면: {5}"),
    DE("gefüllte Löcher: {0} (kleiner als {1} ihrer Komponente oder höchstens "
       "{2} Kanten), hinzugefügte Flächen: {3} -- Ecken: {4}, Flächen: {5}"),
    FR("trous comblés : {0} (plus petits que {1} de leur composante, ou au plus "
       "{2} arêtes), faces ajoutées : {3} -- sommets : {4}, faces : {5}"),
    ES("agujeros rellenados: {0} (menores que {1} de su componente, o con {2} "
       "aristas como mucho), caras añadidas: {3}: vértices: {4}, caras: {5}"),
    PT("buracos preenchidos: {0} (menores que {1} do seu componente, ou com no "
       "máximo {2} arestas), faces adicionadas: {3} -- vértices: {4}, faces: {5}"),
    IT("buchi chiusi: {0} (più piccoli di {1} della loro componente, o al "
       "massimo {2} spigoli), facce aggiunte: {3} -- vertici: {4}, facce: {5}"),
    NL("gevulde gaten: {0} (kleiner dan {1} van hun component, of hoogstens {2} "
       "randen), toegevoegde vlakken: {3} -- hoekpunten: {4}, vlakken: {5}"),
    RU("залатано отверстий: {0} (меньше {1} своей компоненты либо не более {2} "
       "рёбер), добавлено граней: {3} -- вершины: {4}, грани: {5}"),
    TR("kapatılan delik: {0} (bileşeninin {1} kadarından küçük ya da en çok {2} "
       "kenar), eklenen yüz: {3} -- köşe: {4}, yüz: {5}"));

SS_MSG(nonmanifold_split,
    EN("non-manifold vertices split: {0}, copies added: {1} -- vertices: {2}"),
    JA("分割した非多様体頂点: {0}、加えた複製: {1} -- 頂点: {2}"),
    ZH_HANS("拆开的非流形顶点：{0}，新增的副本：{1} —— 顶点：{2}"),
    ZH_HANT("拆開的非流形頂點：{0}，新增的副本：{1} —— 頂點：{2}"),
    KO("쪼갠 비다양체 정점: {0}, 더한 복제: {1} -- 정점: {2}"),
    DE("aufgetrennte nicht-mannigfaltige Ecken: {0}, hinzugefügte Kopien: {1} "
       "-- Ecken: {2}"),
    FR("sommets non variétés séparés : {0}, copies ajoutées : {1} -- "
       "sommets : {2}"),
    ES("vértices no variedad separados: {0}, copias añadidas: {1}: "
       "vértices: {2}"),
    PT("vértices não variedade separados: {0}, cópias adicionadas: {1} -- "
       "vértices: {2}"),
    IT("vertici non-varietà separati: {0}, copie aggiunte: {1} -- vertici: {2}"),
    NL("gesplitste niet-variëteitshoekpunten: {0}, toegevoegde kopieën: {1} -- "
       "hoekpunten: {2}"),
    RU("разделено немногообразных вершин: {0}, добавлено копий: {1} -- "
       "вершины: {2}"),
    TR("ayrılan çok-katmanlı olmayan köşe: {0}, eklenen kopya: {1} -- köşe: {2}"));

SS_MSG(degenerate_none,
    EN("degenerate faces: none"),
    JA("退化した面: なし"),
    ZH_HANS("退化面：没有"),
    ZH_HANT("退化面：沒有"),
    KO("퇴화한 면: 없음"),
    DE("entartete Flächen: keine"),
    FR("faces dégénérées : aucune"),
    ES("caras degeneradas: ninguna"),
    PT("faces degeneradas: nenhuma"),
    IT("facce degeneri: nessuna"),
    NL("ontaarde vlakken: geen"),
    RU("вырожденные грани: нет"),
    TR("bozuk yüz: yok"));

SS_MSG(degenerate_fixed,
    EN("degenerate faces -- collapses: {0}, flips: {1}; vertices: {2}, "
       "faces: {3}"),
    JA("退化した面 -- 縮約: {0}、反転: {1}、頂点: {2}、面: {3}"),
    ZH_HANS("退化面 —— 坍缩：{0}，翻转：{1}；顶点：{2}，面：{3}"),
    ZH_HANT("退化面 —— 塌縮：{0}，翻轉：{1}；頂點：{2}，面：{3}"),
    KO("퇴화한 면 -- 축약: {0}, 뒤집기: {1}; 정점: {2}, 면: {3}"),
    DE("entartete Flächen -- Kollapse: {0}, Umklappungen: {1}; Ecken: {2}, "
       "Flächen: {3}"),
    FR("faces dégénérées -- effondrements : {0}, bascules : {1} ; sommets : {2}, "
       "faces : {3}"),
    ES("caras degeneradas: colapsos: {0}, volteos: {1}; vértices: {2}, "
       "caras: {3}"),
    PT("faces degeneradas -- colapsos: {0}, inversões: {1}; vértices: {2}, "
       "faces: {3}"),
    IT("facce degeneri -- collassi: {0}, ribaltamenti: {1}; vertici: {2}, "
       "facce: {3}"),
    NL("ontaarde vlakken -- inklappingen: {0}, omklappingen: {1}; "
       "hoekpunten: {2}, vlakken: {3}"),
    RU("вырожденные грани -- схлопываний: {0}, перекладываний: {1}; "
       "вершины: {2}, грани: {3}"),
    TR("bozuk yüzler -- çökertme: {0}, çevirme: {1}; köşe: {2}, yüz: {3}"));

SS_MSG(quality_result,
    EN("valence flips: {0}, tangential moves: {1}, iterations: {2}"),
    JA("価数の反転: {0}、接線方向の移動: {1}、反復: {2}"),
    ZH_HANS("价数翻转：{0}，切向移动：{1}，迭代：{2}"),
    ZH_HANT("價數翻轉：{0}，切向移動：{1}，疊代：{2}"),
    KO("차수 뒤집기: {0}, 접선 방향 이동: {1}, 반복: {2}"),
    DE("Valenz-Umklappungen: {0}, tangentiale Verschiebungen: {1}, "
       "Iterationen: {2}"),
    FR("bascules de valence : {0}, déplacements tangentiels : {1}, "
       "itérations : {2}"),
    ES("volteos de valencia: {0}, desplazamientos tangenciales: {1}, "
       "iteraciones: {2}"),
    PT("inversões de valência: {0}, deslocamentos tangenciais: {1}, "
       "iterações: {2}"),
    IT("ribaltamenti di valenza: {0}, spostamenti tangenziali: {1}, "
       "iterazioni: {2}"),
    NL("valentie-omklappingen: {0}, tangentiële verplaatsingen: {1}, "
       "iteraties: {2}"),
    RU("перекладываний по валентности: {0}, касательных сдвигов: {1}, "
       "итерации: {2}"),
    TR("değerlik çevirmesi: {0}, teğetsel kaydırma: {1}, yineleme: {2}"));

// ===========================================================================
// Writing the result
// ===========================================================================

SS_MSG(wrote_file,
    EN("{0} -- vertices: {1}, faces: {2}"),
    JA("{0} -- 頂点: {1}、面: {2}"),
    ZH_HANS("{0} —— 顶点：{1}，面：{2}"),
    ZH_HANT("{0} —— 頂點：{1}，面：{2}"),
    KO("{0} -- 정점: {1}, 면: {2}"),
    DE("{0} -- Ecken: {1}, Flächen: {2}"),
    FR("{0} -- sommets : {1}, faces : {2}"),
    ES("{0}: vértices: {1}, caras: {2}"),
    PT("{0} -- vértices: {1}, faces: {2}"),
    IT("{0} -- vertici: {1}, facce: {2}"),
    NL("{0} -- hoekpunten: {1}, vlakken: {2}"),
    RU("{0} -- вершины: {1}, грани: {2}"),
    TR("{0} -- köşe: {1}, yüz: {2}"));

SS_MSG(stats_empty,
    EN("the mesh is empty"),
    JA("メッシュが空です"),
    ZH_HANS("网格是空的"),
    ZH_HANT("網格是空的"),
    KO("메시가 비어 있습니다"),
    DE("das Netz ist leer"),
    FR("le maillage est vide"),
    ES("la malla está vacía"),
    PT("a malha está vazia"),
    IT("la mesh è vuota"),
    NL("de mesh is leeg"),
    RU("меш пуст"),
    TR("ağ boş"));

SS_MSG(stats_line,
    EN("- vertices: {0}, faces: {1}, components: {2}; edges -- boundary: {3}, non-manifold: {4}, mis-oriented: {5};\n- duplicate faces: {6}; smallest angle: {7} deg (faces under 5 deg: {8}, under 15 deg: {9})"),
    JA("頂点: {0}、面: {1}、連結成分: {2}、辺 -- 境界: {3}、非多様体: {4}、向き不整合: {5}、重複面: {6}、最小角: {7} 度（5 度未満の面: {8}、15 度未満: {9}）"),
    ZH_HANS("- 顶点：{0}，面：{1}，连通块：{2}； 边 —— 边界：{3}，非流形：{4}，朝向不一致：{5}； 重复面：{6}；\n- 最小角：{7} 度（小于 5 度的面：{8}，小于 15 度：{9}）"),
    ZH_HANT("- 頂點：{0}，面：{1}，連通塊：{2}； 邊 —— 邊界：{3}，非流形：{4}，朝向不一致：{5}； 重複面：{6}；\n- 最小角：{7} 度（小於 5 度的面：{8}，小於 15 度：{9}）"),
    KO("- 정점: {0}, 면: {1}, 연결 성분: {2}; 모서리 -- 경계: {3}, 비다양체: {4}, 방향 어긋남: {5}; 중복 면: {6};\n- 최소 각: {7}도(5도 미만 면: {8}, 15도 미만: {9})"),
    DE("- Ecken: {0}, Flächen: {1}, Komponenten: {2};\n- Kanten -- Rand: {3}, nicht-mannigfaltig: {4}, falsch orientiert: {5}; doppelte Flächen: {6};\n- kleinster Winkel: {7} Grad (Flächen unter 5 Grad: {8}, unter 15 Grad: {9})"),
    FR("- sommets : {0}, faces : {1}, composantes : {2} ; arêtes -- bord : {3}, non variétés : {4}, mal orientées : {5} ;\n- faces en double : {6} ; plus petit angle : {7} degrés (faces sous 5 degrés : {8}, sous 15 degrés : {9})"),
    ES("- vértices: {0}, caras: {1}, componentes: {2}; aristas: de borde: {3}, no variedad: {4}, mal orientadas: {5};\n- caras duplicadas: {6}; ángulo menor: {7} grados (caras por debajo de 5 grados: {8}, por debajo de 15 grados: {9})"),
    PT("- vértices: {0}, faces: {1}, componentes: {2}; arestas -- de borda: {3}, não variedade: {4}, mal orientadas: {5};\n- faces duplicadas: {6}; menor ângulo: {7} graus (faces abaixo de 5 graus: {8}, abaixo de 15 graus: {9})"),
    IT("- vertici: {0}, facce: {1}, componenti: {2}; spigoli -- di bordo: {3}, non-varietà: {4}, mal orientati: {5};\n- facce duplicate: {6}; angolo minimo: {7} gradi (facce sotto 5 gradi: {8}, sotto 15 gradi: {9})"),
    NL("- hoekpunten: {0}, vlakken: {1}, componenten: {2};\n- randen -- rand: {3}, niet-variëteit: {4}, verkeerd georiënteerd: {5}; dubbele vlakken: {6};\n- kleinste hoek: {7} graden (vlakken onder 5 graden: {8}, onder 15 graden: {9})"),
    RU("- вершины: {0}, грани: {1}, компоненты: {2};\n- рёбра -- граничные: {3}, немногообразные: {4}, с неверной ориентацией: {5}; дублирующие грани: {6};\n- наименьший угол: {7} градусов (грани меньше 5 градусов: {8}, меньше 15 градусов: {9})"),
    TR("- köşe: {0}, yüz: {1}, bileşen: {2}; kenarlar -- sınır: {3}, çok-katmanlı olmayan: {4}, yönü yanlış: {5};\n- yinelenen yüz: {6}; en küçük açı: {7} derece (5 derecenin altındaki yüz: {8}, 15 derecenin altında: {9})"));

SS_MSG(done_summary,
    EN("vertices: {0}, faces: {1} (total {2}s)"),
    JA("頂点: {0}、面: {1}（合計 {2}秒）"),
    ZH_HANS("顶点：{0}，面：{1}（共 {2} 秒）"),
    ZH_HANT("頂點：{0}，面：{1}（共 {2} 秒）"),
    KO("정점: {0}, 면: {1}(합계 {2}초)"),
    DE("Ecken: {0}, Flächen: {1} (insgesamt {2} s)"),
    FR("sommets : {0}, faces : {1} (total {2} s)"),
    ES("vértices: {0}, caras: {1} (total {2} s)"),
    PT("vértices: {0}, faces: {1} (total {2} s)"),
    IT("vertici: {0}, facce: {1} (totale {2} s)"),
    NL("hoekpunten: {0}, vlakken: {1} (totaal {2} s)"),
    RU("вершины: {0}, грани: {1} (всего {2} с)"),
    TR("köşe: {0}, yüz: {1} (toplam {2} sn)"));

// ===========================================================================
// UV atlas and texture baking
// ===========================================================================

SS_MSG(uv_grew,
    EN("charts grown: {0} (cap {1}), after the disk split: {2}"),
    JA("広げたチャート: {0}（上限 {1}）、円板分割後: {2}"),
    ZH_HANS("扩张的图块：{0}（上限 {1}），圆盘拆分后：{2}"),
    ZH_HANT("擴張的圖塊：{0}（上限 {1}），圓盤拆分後：{2}"),
    KO("키운 차트: {0}(상한 {1}), 원판 분할 후: {2}"),
    DE("gewachsene Charts: {0} (Obergrenze {1}), nach der Scheibenteilung: {2}"),
    FR("charts étendus : {0} (plafond {1}), après la découpe en disques : {2}"),
    ES("cartas ampliadas: {0} (tope {1}), tras la división en discos: {2}"),
    PT("cartas expandidas: {0} (limite {1}), após a divisão em discos: {2}"),
    IT("chart estesi: {0} (tetto {1}), dopo la divisione a disco: {2}"),
    NL("gegroeide charts: {0} (plafond {1}), na de schijfsplitsing: {2}"),
    RU("выращено карт: {0} (предел {1}), после разрезания на диски: {2}"),
    TR("büyütülen parça: {0} (üst sınır {1}), disk bölmesinden sonra: {2}"));

SS_MSG(uv_auto_size,
    EN("texture size chosen for you: {0} (budget {1} texels)"),
    JA("自動で選んだテクスチャサイズ: {0}（予算 {1} テクセル）"),
    ZH_HANS("自动选定的纹理尺寸：{0}（预算 {1} 纹素）"),
    ZH_HANT("自動選定的紋理尺寸：{0}（預算 {1} 紋素）"),
    KO("자동으로 고른 텍스처 크기: {0}(예산 {1} 텍셀)"),
    DE("gewählte Texturgröße: {0} (Budget {1} Texel)"),
    FR("taille de texture choisie : {0} (budget {1} texels)"),
    ES("tamaño de textura elegido: {0} (presupuesto {1} téxeles)"),
    PT("tamanho de textura escolhido: {0} (orçamento {1} texels)"),
    IT("dimensione di texture scelta: {0} (budget {1} texel)"),
    NL("gekozen textuurgrootte: {0} (budget {1} texels)"),
    RU("выбранный размер текстуры: {0} (бюджет {1} текселей)"),
    TR("seçilen doku boyutu: {0} (bütçe {1} teksel)"));

SS_MSG(uv_charts,
    EN("charts: {0} over faces: {1} ({2}s)"),
    JA("チャート: {0}、対象の面: {1}（{2}秒）"),
    ZH_HANS("图块：{0}，覆盖的面：{1}（{2} 秒）"),
    ZH_HANT("圖塊：{0}，涵蓋的面：{1}（{2} 秒）"),
    KO("차트: {0}, 대상 면: {1}({2}초)"),
    DE("Charts: {0} über Flächen: {1} ({2} s)"),
    FR("charts : {0} sur faces : {1} ({2} s)"),
    ES("cartas: {0} sobre caras: {1} ({2} s)"),
    PT("cartas: {0} sobre faces: {1} ({2} s)"),
    IT("chart: {0} su facce: {1} ({2} s)"),
    NL("charts: {0} over vlakken: {1} ({2} s)"),
    RU("карты: {0}, охваченные грани: {1} ({2} с)"),
    TR("parça: {0}, kapsanan yüz: {1} ({2} sn)"));

SS_MSG(uv_param,
    EN("charts parameterized: {0} (split retries: {1}, parked: {2}) ({3}s)"),
    JA("パラメータ化したチャート: {0}（分割やり直し: {1}、保留: {2}）（{3}秒）"),
    ZH_HANS("参数化的图块：{0}（拆分重试：{1}，搁置：{2}）（{3} 秒）"),
    ZH_HANT("參數化的圖塊：{0}（拆分重試：{1}，擱置：{2}）（{3} 秒）"),
    KO("매개변수화한 차트: {0}(분할 재시도: {1}, 보류: {2})({3}초)"),
    DE("parametrisierte Charts: {0} (Teilungsversuche: {1}, "
       "zurückgestellt: {2}) ({3} s)"),
    FR("charts paramétrés : {0} (nouvelles tentatives de découpe : {1}, mis de "
       "côté : {2}) ({3} s)"),
    ES("cartas parametrizadas: {0} (reintentos de división: {1}, "
       "apartadas: {2}) ({3} s)"),
    PT("cartas parametrizadas: {0} (novas tentativas de divisão: {1}, "
       "postas de lado: {2}) ({3} s)"),
    IT("chart parametrizzati: {0} (nuovi tentativi di divisione: {1}, messi da "
       "parte: {2}) ({3} s)"),
    NL("geparametriseerde charts: {0} (nieuwe splitspogingen: {1}, "
       "opzijgezet: {2}) ({3} s)"),
    RU("параметризовано карт: {0} (повторов разрезания: {1}, отложено: {2}) "
       "({3} с)"),
    TR("parametrelenen parça: {0} (bölme yeniden denemesi: {1}, "
       "bekletilen: {2}) ({3} sn)"));

SS_MSG(uv_packed,
    EN("charts packed: {0}, efficiency {1}%, texels per budget unit: {2} ({3}s)"),
    JA("詰めたチャート: {0}、充填率 {1}%、予算単位あたりのテクセル: {2}（{3}秒）"),
    ZH_HANS("排布的图块：{0}，利用率 {1}%，每预算单位纹素：{2}（{3} 秒）"),
    ZH_HANT("排布的圖塊：{0}，利用率 {1}%，每預算單位紋素：{2}（{3} 秒）"),
    KO("채운 차트: {0}, 효율 {1}%, 예산 단위당 텍셀: {2}({3}초)"),
    DE("gepackte Charts: {0}, Ausnutzung {1} %, Texel je Budgeteinheit: {2} "
       "({3} s)"),
    FR("charts placés : {0}, rendement {1} %, texels par unité de budget : {2} "
       "({3} s)"),
    ES("cartas empaquetadas: {0}, aprovechamiento {1} %, téxeles por unidad de "
       "presupuesto: {2} ({3} s)"),
    PT("cartas empacotadas: {0}, aproveitamento {1}%, texels por unidade de "
       "orçamento: {2} ({3} s)"),
    IT("chart impacchettati: {0}, resa {1}%, texel per unità di budget: {2} "
       "({3} s)"),
    NL("ingepakte charts: {0}, benutting {1}%, texels per budgeteenheid: {2} "
       "({3} s)"),
    RU("упаковано карт: {0}, заполнение {1} %, текселей на единицу "
       "бюджета: {2} ({3} с)"),
    TR("yerleştirilen parça: {0}, verim %{1}, bütçe birimi başına teksel: {2} "
       "({3} sn)"));

SS_MSG(uv_seam,
    EN("seam split -- vertices added: {0}, vertices: {1} ({2}s in total)"),
    JA("シーム分割 -- 加えた頂点: {0}、頂点: {1}（合計 {2}秒）"),
    ZH_HANS("接缝拆分 —— 新增的顶点：{0}，顶点：{1}（共 {2} 秒）"),
    ZH_HANT("接縫拆分 —— 新增的頂點：{0}，頂點：{1}（共 {2} 秒）"),
    KO("솔기 분할 -- 더한 정점: {0}, 정점: {1}(합계 {2}초)"),
    DE("Nahttrennung -- hinzugefügte Ecken: {0}, Ecken: {1} (insgesamt {2} s)"),
    FR("découpe des coutures -- sommets ajoutés : {0}, sommets : {1} (total "
       "{2} s)"),
    ES("corte de costuras: vértices añadidos: {0}, vértices: {1} (total {2} s)"),
    PT("corte das costuras -- vértices adicionados: {0}, vértices: {1} (total "
       "{2} s)"),
    IT("taglio delle cuciture -- vertici aggiunti: {0}, vertici: {1} (in totale "
       "{2} s)"),
    NL("naadsplitsing -- toegevoegde hoekpunten: {0}, hoekpunten: {1} (totaal "
       "{2} s)"),
    RU("разрез по швам -- добавлено вершин: {0}, вершины: {1} (всего {2} с)"),
    TR("dikiş ayrımı -- eklenen köşe: {0}, köşe: {1} (toplam {2} sn)"));

SS_MSG(bake_covered,
    EN("covered texels: {0}/{1} ({2}%) ({3}s)"),
    JA("覆われたテクセル: {0}/{1}（{2}%）（{3}秒）"),
    ZH_HANS("覆盖到的纹素：{0}/{1}（{2}%）（{3} 秒）"),
    ZH_HANT("覆蓋到的紋素：{0}/{1}（{2}%）（{3} 秒）"),
    KO("덮인 텍셀: {0}/{1}({2}%)({3}초)"),
    DE("belegte Texel: {0}/{1} ({2} %) ({3} s)"),
    FR("texels couverts : {0}/{1} ({2} %) ({3} s)"),
    ES("téxeles cubiertos: {0}/{1} ({2} %) ({3} s)"),
    PT("texels cobertos: {0}/{1} ({2}%) ({3} s)"),
    IT("texel coperti: {0}/{1} ({2}%) ({3} s)"),
    NL("gedekte texels: {0}/{1} ({2}%) ({3} s)"),
    RU("покрытые тексели: {0}/{1} ({2} %) ({3} с)"),
    TR("kaplanan teksel: {0}/{1} (%{2}) ({3} sn)"));

SS_MSG(bake_colorized,
    EN("texels colored: {0} ({1}s)"),
    JA("色をつけたテクセル: {0}（{1}秒）"),
    ZH_HANS("上色的纹素：{0}（{1} 秒）"),
    ZH_HANT("上色的紋素：{0}（{1} 秒）"),
    KO("색을 입힌 텍셀: {0}({1}초)"),
    DE("eingefärbte Texel: {0} ({1} s)"),
    FR("texels colorés : {0} ({1} s)"),
    ES("téxeles coloreados: {0} ({1} s)"),
    PT("texels coloridos: {0} ({1} s)"),
    IT("texel colorati: {0} ({1} s)"),
    NL("gekleurde texels: {0} ({1} s)"),
    RU("окрашено текселей: {0} ({1} с)"),
    TR("renklendirilen teksel: {0} ({1} sn)"));

SS_MSG(bake_done,
    EN("dilation and fill ({0}s); the {1}x{2} texture is finished ({3}s in "
       "total)"),
    JA("膨張と穴埋め（{0}秒）、{1}x{2} のテクスチャができました（合計 {3}秒）"),
    ZH_HANS("扩散与填充（{0} 秒）；{1}x{2} 的纹理已完成（共 {3} 秒）"),
    ZH_HANT("擴散與填充（{0} 秒）；{1}x{2} 的紋理已完成（共 {3} 秒）"),
    KO("확장과 채우기({0}초); {1}x{2} 텍스처가 끝났습니다(합계 {3}초)"),
    DE("Ausdehnung und Füllung ({0} s); die {1}x{2}-Textur ist fertig "
       "(insgesamt {3} s)"),
    FR("dilatation et remplissage ({0} s) ; la texture {1}x{2} est terminée "
       "(total {3} s)"),
    ES("dilatación y relleno ({0} s); la textura de {1}x{2} está terminada "
       "(total {3} s)"),
    PT("dilatação e preenchimento ({0} s); a textura {1}x{2} está pronta "
       "(total {3} s)"),
    IT("dilatazione e riempimento ({0} s); la texture {1}x{2} è pronta (in "
       "totale {3} s)"),
    NL("verbreding en opvulling ({0} s); de textuur van {1}x{2} is klaar "
       "(totaal {3} s)"),
    RU("расширение и заливка ({0} с); текстура {1}x{2} готова (всего {3} с)"),
    TR("genişletme ve doldurma ({0} sn); {1}x{2} doku bitti (toplam {3} sn)"));

SS_MSG(debug_render_unavailable,
    EN("the debug render needs camera intrinsics, and there are none"),
    JA("デバッグ描画にはカメラ内部パラメータが要りますが、ありません"),
    ZH_HANS("调试渲染需要相机内参，但没有"),
    ZH_HANT("除錯算繪需要相機內參，但沒有"),
    KO("디버그 렌더링에는 카메라 내부 파라미터가 필요한데 없습니다"),
    DE("das Debug-Rendering braucht Kamera-Intrinsics, und es gibt keine"),
    FR("le rendu de débogage a besoin de paramètres internes, et il n'y en a "
       "aucun"),
    ES("el renderizado de depuración necesita parámetros internos, y no hay "
       "ninguno"),
    PT("a renderização de depuração precisa de parâmetros internos, e não há "
       "nenhum"),
    IT("il rendering di debug ha bisogno dei parametri interni, e non ce ne "
       "sono"),
    NL("de debugrender heeft camera-intrinsieken nodig, en die zijn er niet"),
    RU("отладочной отрисовке нужны внутренние параметры камеры, а их нет"),
    TR("hata ayıklama işlemesi kamera iç parametreleri istiyor, ama yok"));

// ===========================================================================
// `spirula mesh --help`
// ===========================================================================
// The flag names, their value syntax and their defaults are identifiers and
// stay as they are; what is translated is the sentence beside each one. A
// terminal has no language picker to fix afterwards, which is the whole reason
// the command line is in the catalogs at all.

SS_MSG(help_usage,
    EN("usage: {0} <checkpoint> [--flag value ...]"),
    JA("使い方: {0} <checkpoint> [--flag value ...]"),
    ZH_HANS("用法：{0} <checkpoint> [--flag value ...]"),
    ZH_HANT("用法：{0} <checkpoint> [--flag value ...]"),
    KO("사용법: {0} <checkpoint> [--flag value ...]"),
    DE("Aufruf: {0} <checkpoint> [--flag value ...]"),
    FR("utilisation : {0} <checkpoint> [--flag value ...]"),
    ES("uso: {0} <checkpoint> [--flag value ...]"),
    PT("uso: {0} <checkpoint> [--flag value ...]"),
    IT("uso: {0} <checkpoint> [--flag value ...]"),
    NL("gebruik: {0} <checkpoint> [--flag value ...]"),
    RU("использование: {0} <checkpoint> [--flag value ...]"),
    TR("kullanım: {0} <checkpoint> [--flag value ...]"));

SS_MSG(help_intro,
    EN("Extract a surface mesh from 3DGS. <checkpoint>: run folder (config.json + step-*.ckpt/), *.ckpt folder or splat.ply."),
    JA("3DGSから表面メッシュを抽出。<checkpoint>：実行フォルダ（config.json + step-*.ckpt/）、*.ckpt フォルダ、または splat.ply。"),
    ZH_HANS("从 3DGS 提取表面网格。<checkpoint>：运行目录（config.json + step-*.ckpt/）、*.ckpt 目录或 splat.ply。"),
    ZH_HANT("從 3DGS 擷取表面網格。<checkpoint>：執行目錄（config.json + step-*.ckpt/）、*.ckpt 目錄或 splat.ply。"),
    KO("3DGS에서 표면 메시 추출. <checkpoint>: 실행 폴더(config.json + step-*.ckpt/), *.ckpt 폴더 또는 splat.ply."),
    DE("Oberflächen-Mesh aus 3DGS extrahieren. <checkpoint>: Lauf-Ordner (config.json + step-*.ckpt/), *.ckpt-Ordner oder splat.ply."),
    FR("Extraire un maillage de 3DGS. <checkpoint> : dossier d’exécution (config.json + step-*.ckpt/), dossier *.ckpt ou splat.ply."),
    ES("Extrae una malla de 3DGS. <checkpoint>: carpeta de ejecución (config.json + step-*.ckpt/), carpeta *.ckpt o splat.ply."),
    PT("Extrai uma malha de 3DGS. <checkpoint>: pasta de execução (config.json + step-*.ckpt/), pasta *.ckpt ou splat.ply."),
    IT("Estrai una mesh da 3DGS. <checkpoint>: cartella di esecuzione (config.json + step-*.ckpt/), cartella *.ckpt o splat.ply."),
    NL("Extraheer een oppervlakmesh uit 3DGS. <checkpoint>: run-map (config.json + step-*.ckpt/), *.ckpt-map of splat.ply."),
    RU("Извлечь сетку из 3DGS. <checkpoint>: папка запуска (config.json + step-*.ckpt/), папка *.ckpt или splat.ply."),
    TR("3DGS’den yüzey ağı çıkarır. <checkpoint>: çalışma klasörü (config.json + step-*.ckpt/), *.ckpt klasörü veya splat.ply."));

SS_MSG(help_data,
    EN("dataset for camera-based occupancy and colors\n"
       "(default: the `data` recorded in config.json)"),
    JA("カメラを使う占有度と色のためのデータセット\n"
       "（既定: config.json に記録された `data`）"),
    ZH_HANS("用于基于相机的占据场与颜色的数据集\n"
            "（默认：config.json 中记录的 `data`）"),
    ZH_HANT("用於基於相機的佔據場與顏色的資料集\n"
            "（預設：config.json 中記錄的 `data`）"),
    KO("카메라를 쓰는 점유도와 색을 위한 데이터셋\n"
       "(기본값: config.json 에 적힌 `data`)"),
    DE("Datensatz für kamerabasierte Belegung und Farben\n"
       "(Vorgabe: das in config.json vermerkte `data`)"),
    FR("jeu de données pour l'occupation et les couleurs par caméra\n"
       "(défaut : le `data` inscrit dans config.json)"),
    ES("conjunto de datos para la ocupación y los colores por cámara\n"
       "(por defecto: el `data` anotado en config.json)"),
    PT("conjunto de dados para a ocupação e as cores por câmera\n"
       "(padrão: o `data` anotado em config.json)"),
    IT("dataset per l'occupazione e i colori basati sulle camere\n"
       "(predefinito: il `data` annotato in config.json)"),
    NL("dataset voor camerabepaalde bezetting en kleuren\n"
       "(standaard: de `data` uit config.json)"),
    RU("набор данных для заполненности и цветов по камерам\n"
       "(по умолчанию: `data` из config.json)"),
    TR("kamera tabanlı doluluk ve renkler için veri kümesi\n"
       "(varsayılan: config.json içindeki `data`)"));

SS_MSG(help_no_data,
    EN("mesh from the Gaussian densities alone"),
    JA("ガウシアンの密度だけからメッシュを作る"),
    ZH_HANS("只依据高斯球的密度生成网格"),
    ZH_HANT("只依據高斯球的密度產生網格"),
    KO("가우시안 밀도만으로 메시를 만듭니다"),
    DE("nur aus den Gaußdichten vernetzen"),
    FR("mailler à partir des seules densités gaussiennes"),
    ES("mallar solo a partir de las densidades gaussianas"),
    PT("gerar a malha só a partir das densidades gaussianas"),
    IT("costruire la mesh dalle sole densità gaussiane"),
    NL("mesh alleen uit de gaussische dichtheden"),
    RU("строить меш только по плотностям гауссиан"),
    TR("yalnızca gaussian yoğunluklarından ağ üret"));

SS_MSG(help_data_format,
    EN("colmap|nerfstudio|metashape (default: work it out)"),
    JA("colmap|nerfstudio|metashape（既定: 自動判別）"),
    ZH_HANS("colmap|nerfstudio|metashape（默认：自动判别）"),
    ZH_HANT("colmap|nerfstudio|metashape（預設：自動判別）"),
    KO("colmap|nerfstudio|metashape(기본값: 자동 판별)"),
    DE("colmap|nerfstudio|metashape (Vorgabe: selbst erkennen)"),
    FR("colmap|nerfstudio|metashape (défaut : deviner)"),
    ES("colmap|nerfstudio|metashape (por defecto: averiguarlo)"),
    PT("colmap|nerfstudio|metashape (padrão: descobrir sozinho)"),
    IT("colmap|nerfstudio|metashape (predefinito: capirlo da sé)"),
    NL("colmap|nerfstudio|metashape (standaard: zelf uitzoeken)"),
    RU("colmap|nerfstudio|metashape (по умолчанию: определить самому)"),
    TR("colmap|nerfstudio|metashape (varsayılan: kendi bulsun)"));

SS_MSG(help_device,
    EN("GPU to run on: an index, part of a name, `auto`, or uuid:<32 hex digits> (default: the environment, then auto)"),
    JA("使用する GPU: 番号、名前の一部、`auto`、 または uuid:<32 桁の 16 進数>（既定: 環境変数、次に自動）"),
    ZH_HANS("要使用的 GPU：序号、名称的一部分、`auto`， 或 uuid:<32 位十六进制>（默认：环境变量，然后自动）"),
    ZH_HANT("要使用的 GPU：序號、名稱的一部分、`auto`， 或 uuid:<32 位十六進位>（預設：環境變數，然後自動）"),
    KO("사용할 GPU: 번호, 이름의 일부, `auto`, 또는 uuid:<32자리 16진수> (기본값: 환경 변수, 그다음 자동)"),
    DE("GPU, auf der gerechnet wird: ein Index, ein Teil des Namens, `auto` oder uuid:<32 Hexziffern> (Vorgabe: die Umgebung, dann automatisch)"),
    FR("GPU à utiliser : un indice, une partie d'un nom, `auto`, ou uuid:<32 chiffres hexadécimaux> (défaut : l'environnement, puis auto)"),
    ES("GPU que se usará: un índice, parte de un nombre, `auto`, o uuid:<32 dígitos hexadecimales> (por defecto: el entorno, luego auto)"),
    PT("GPU a usar: um índice, parte de um nome, `auto`, ou uuid:<32 dígitos hexadecimais> (padrão: o ambiente, depois auto)"),
    IT("GPU da usare: un indice, parte di un nome, `auto`, o uuid:<32 cifre esadecimali> (predefinito: l'ambiente, poi automatico)"),
    NL("GPU om op te rekenen: een index, deel van een naam, `auto`, of uuid:<32 hexcijfers> (standaard: de omgeving, dan automatisch)"),
    RU("GPU для работы: номер, часть имени, `auto` или uuid:<32 шестнадцатеричные цифры> (по умолчанию: окружение, затем авто)"),
    TR("Üzerinde çalışılacak GPU: bir sıra, adın bir parçası, `auto` veya uuid:<32 onaltılık basamak> (varsayılan: ortam, sonra otomatik)"));
SS_MSG(help_device_cuda,
    EN("CUDA device index to run on (default: the current CUDA device)"),
    JA("使用する CUDA デバイス番号（既定: 現在の CUDA デバイス）"),
    ZH_HANS("要使用的 CUDA 设备序号（默认：当前 CUDA 设备）"),
    ZH_HANT("要使用的 CUDA 裝置序號（預設：目前 CUDA 裝置）"),
    KO("사용할 CUDA 장치 번호(기본값: 현재 CUDA 장치)"),
    DE("CUDA-Geräteindex für die Ausführung (Vorgabe: aktuelles CUDA-Gerät)"),
    FR("indice du périphérique CUDA à utiliser (défaut : périphérique CUDA courant)"),
    ES("índice del dispositivo CUDA que se usará (por defecto: dispositivo CUDA actual)"),
    PT("índice do dispositivo CUDA a usar (padrão: dispositivo CUDA atual)"),
    IT("indice del dispositivo CUDA da usare (predefinito: dispositivo CUDA corrente)"),
    NL("CUDA-apparaatindex om op te draaien (standaard: huidige CUDA-apparaat)"),
    RU("индекс CUDA-устройства (по умолчанию: текущее CUDA-устройство)"),
    TR("çalıştırılacak CUDA aygıtı dizini (varsayılan: geçerli CUDA aygıtı)"));

SS_MSG(help_output,
    EN("output base path (default: <checkpoint>/mesh);\n"
       "a known extension is stripped"),
    JA("出力の基準パス（既定: <checkpoint>/mesh）。\n"
       "既知の拡張子は取り除かれます"),
    ZH_HANS("输出基准路径（默认：<checkpoint>/mesh）；\n"
            "已知的扩展名会被去掉"),
    ZH_HANT("輸出基準路徑（預設：<checkpoint>/mesh）；\n"
            "已知的副檔名會被去掉"),
    KO("출력 기준 경로(기본값: <checkpoint>/mesh);\n"
       "아는 확장자는 떼어냅니다"),
    DE("Basispfad der Ausgabe (Vorgabe: <checkpoint>/mesh);\n"
       "eine bekannte Endung wird abgeschnitten"),
    FR("chemin de base de la sortie (défaut : <checkpoint>/mesh) ;\n"
       "une extension connue est retirée"),
    ES("ruta base de salida (por defecto: <checkpoint>/mesh);\n"
       "se quita una extensión conocida"),
    PT("caminho base de saída (padrão: <checkpoint>/mesh);\n"
       "uma extensão conhecida é removida"),
    IT("percorso base dell'output (predefinito: <checkpoint>/mesh);\n"
       "un'estensione nota viene tolta"),
    NL("basispad van de uitvoer (standaard: <checkpoint>/mesh);\n"
       "een bekende extensie wordt weggehaald"),
    RU("базовый путь вывода (по умолчанию: <checkpoint>/mesh);\n"
       "известное расширение отбрасывается"),
    TR("çıktı taban yolu (varsayılan: <checkpoint>/mesh);\n"
       "bilinen bir uzantı kırpılır"));

SS_MSG(help_format,
    EN("comma-separated: ply,obj,gltf,glb,stl [{0}]. With --color texture a format may carry a texture encoding: glb+png (default), glb+jpg (JPEG q95), glb+jpeg75 (JPEG q75)"),
    JA("カンマ区切り: ply,obj,gltf,glb,stl [{0}]。 --color texture のときは形式にテクスチャの符号化を 付けられます: glb+png（既定）、glb+jpg（JPEG q95）、 glb+jpeg75（JPEG q75）"),
    ZH_HANS("逗号分隔：ply,obj,gltf,glb,stl [{0}]。 配合 --color texture 时，格式后可加纹理编码： glb+png（默认）、glb+jpg（JPEG q95）、 glb+jpeg75（JPEG q75）"),
    ZH_HANT("逗號分隔：ply,obj,gltf,glb,stl [{0}]。 搭配 --color texture 時，格式後可加紋理編碼： glb+png（預設）、glb+jpg（JPEG q95）、 glb+jpeg75（JPEG q75）"),
    KO("쉼표로 구분: ply,obj,gltf,glb,stl [{0}]. --color texture 일 때는 형식에 텍스처 부호화를 붙일 수 있습니다: glb+png(기본), glb+jpg(JPEG q95), glb+jpeg75(JPEG q75)"),
    DE("kommagetrennt: ply,obj,gltf,glb,stl [{0}]. Mit --color texture kann ein Format eine Texturkodierung tragen: glb+png (Vorgabe), glb+jpg (JPEG q95), glb+jpeg75 (JPEG q75)"),
    FR("Formats séparés par virgules : ply,obj,gltf,glb,stl [{0}].\n--color texture : glb+png (défaut), glb+jpg (JPEG q95), glb+jpeg75 (JPEG q75)."),
    ES("Formatos separados por comas: ply,obj,gltf,glb,stl [{0}].\n--color texture: glb+png (por defecto), glb+jpg (JPEG q95), glb+jpeg75 (JPEG q75)."),
    PT("Formatos separados por vírgulas: ply,obj,gltf,glb,stl [{0}].\n--color texture: glb+png (padrão), glb+jpg (JPEG q95), glb+jpeg75 (JPEG q75)."),
    IT("Formati separati da virgole: ply,obj,gltf,glb,stl [{0}].\n--color texture: glb+png (predefinito), glb+jpg (JPEG q95), glb+jpeg75 (JPEG q75)."),
    NL("kommagescheiden: ply,obj,gltf,glb,stl [{0}]. Met --color texture kan een formaat een textuurcodering dragen: glb+png (standaard), glb+jpg (JPEG q95), glb+jpeg75 (JPEG q75)"),
    RU("через запятую: ply,obj,gltf,glb,stl [{0}]. С --color texture формат может нести кодировку текстуры: glb+png (по умолчанию), glb+jpg (JPEG q95), glb+jpeg75 (JPEG q75)"),
    TR("virgülle ayrılmış: ply,obj,gltf,glb,stl [{0}]. --color texture ile bir biçim doku kodlaması taşıyabilir: glb+png (varsayılan), glb+jpg (JPEG q95), glb+jpeg75 (JPEG q75)"));

SS_MSG(help_color,
    EN("comma-separated: none,vertex,texture [{0}] (one run may write several; a pair no format can carry, such as PLY with a texture, is skipped)"),
    JA("カンマ区切り: none,vertex,texture [{0}] （1 回の実行で複数書けます。PLY にテクスチャのように 形式が持てない組み合わせは飛ばします）"),
    ZH_HANS("逗号分隔：none,vertex,texture [{0}] （一次运行可写多个；格式装不下的组合， 例如 PLY 配纹理，会跳过）"),
    ZH_HANT("逗號分隔：none,vertex,texture [{0}] （一次執行可寫多個；格式裝不下的組合， 例如 PLY 配紋理，會跳過）"),
    KO("쉼표로 구분: none,vertex,texture [{0}] (한 번의 실행으로 여러 개를 쓸 수 있으며, 텍스처를 붙인 PLY 처럼 형식이 담을 수 없는 조합은 건너뜁니다)"),
    DE("kommagetrennt: none,vertex,texture [{0}] (ein Lauf kann mehrere schreiben; ein Paar, das kein Format tragen kann, etwa PLY mit Textur, wird übersprungen)"),
    FR("séparés par des virgules : none,vertex,texture [{0}] (une exécution peut en écrire plusieurs ; une paire qu'aucun format ne porte, comme PLY avec texture, est ignorée)"),
    ES("separados por comas: none,vertex,texture [{0}] (una ejecución puede escribir varios; un par que ningún formato admite, como PLY con textura, se omite)"),
    PT("separados por vírgulas: none,vertex,texture [{0}] (uma execução pode escrever vários; um par que nenhum formato aceita, como PLY com textura, é ignorado)"),
    IT("Separati da virgole: none,vertex,texture [{0}]. Un’esecuzione può usarne più di uno; combinazioni incompatibili come PLY con texture sono saltate."),
    NL("kommagescheiden: none,vertex,texture [{0}] (één run kan er meerdere schrijven; een paar dat geen formaat kan dragen, zoals PLY met textuur, wordt overgeslagen)"),
    RU("через запятую: none,vertex,texture [{0}] (за один запуск можно записать несколько; пара, которую не несёт ни один формат, например PLY с текстурой, пропускается)"),
    TR("virgülle ayrılmış: none,vertex,texture [{0}] (bir çalıştırma birkaçını yazabilir; hiçbir biçimin taşıyamadığı bir çift, dokulu PLY gibi, atlanır)"));

SS_MSG(help_texture_size,
    EN("texture atlas resolution; 0 takes it from the\n"
       "observed-detail texel budget [{0}]"),
    JA("テクスチャアトラスの解像度。0 なら観測された細部の\n"
       "テクセル予算から決めます [{0}]"),
    ZH_HANS("纹理图集分辨率；0 表示按观测到的细节的纹素预算决定 [{0}]"),
    ZH_HANT("紋理圖集解析度；0 表示按觀測到的細節的紋素預算決定 [{0}]"),
    KO("텍스처 아틀라스 해상도; 0 이면 관측된 디테일의\n"
       "텍셀 예산에서 정합니다 [{0}]"),
    DE("Auflösung des Texturatlas; 0 nimmt sie aus dem Texelbudget\n"
       "der beobachteten Details [{0}]"),
    FR("résolution de l'atlas de texture ; 0 la prend du budget\n"
       "de texels du détail observé [{0}]"),
    ES("resolución del atlas de textura; 0 la toma del presupuesto\n"
       "de téxeles del detalle observado [{0}]"),
    PT("resolução do atlas de textura; 0 a tira do orçamento de\n"
       "texels do detalhe observado [{0}]"),
    IT("risoluzione dell'atlante di texture; 0 la prende dal budget\n"
       "di texel del dettaglio osservato [{0}]"),
    NL("resolutie van de textuuratlas; 0 haalt hem uit het\n"
       "texelbudget van het waargenomen detail [{0}]"),
    RU("разрешение текстурного атласа; 0 берёт его из бюджета\n"
       "текселей по наблюдаемой детализации [{0}]"),
    TR("doku atlası çözünürlüğü; 0 ise gözlenen ayrıntının\n"
       "teksel bütçesinden alınır [{0}]"));

SS_MSG(help_tex_gutter,
    EN("atlas spacing between UV charts [{0}]"),
    JA("アトラス内で UV チャートの間にあける余白 [{0}]"),
    ZH_HANS("图集中 UV 图块之间的间距 [{0}]"),
    ZH_HANT("圖集中 UV 圖塊之間的間距 [{0}]"),
    KO("아틀라스에서 UV 차트 사이의 여백 [{0}]"),
    DE("Abstand zwischen UV-Charts im Atlas [{0}]"),
    FR("espacement entre charts UV dans l'atlas [{0}]"),
    ES("separación entre cartas UV en el atlas [{0}]"),
    PT("espaçamento entre cartas UV no atlas [{0}]"),
    IT("spaziatura tra chart UV nell'atlante [{0}]"),
    NL("tussenruimte tussen UV-charts in de atlas [{0}]"),
    RU("зазор между UV-картами в атласе [{0}]"),
    TR("atlasta UV parçaları arasındaki boşluk [{0}]"));

SS_MSG(help_chart_angle,
    EN("largest normal deviation within one UV chart [{0}]"),
    JA("1 つの UV チャート内で許す法線のずれの上限 [{0}]"),
    ZH_HANS("同一 UV 图块内允许的最大法线偏差 [{0}]"),
    ZH_HANT("同一 UV 圖塊內允許的最大法線偏差 [{0}]"),
    KO("한 UV 차트 안에서 허용하는 법선 편차의 상한 [{0}]"),
    DE("größte Normalenabweichung innerhalb eines UV-Charts [{0}]"),
    FR("plus grand écart de normale au sein d'un chart UV [{0}]"),
    ES("mayor desviación de normal dentro de una carta UV [{0}]"),
    PT("maior desvio de normal dentro de uma carta UV [{0}]"),
    IT("massimo scarto di normale entro un chart UV [{0}]"),
    NL("grootste normaalafwijking binnen één UV-chart [{0}]"),
    RU("наибольшее отклонение нормали внутри одной UV-карты [{0}]"),
    TR("bir UV parçası içindeki en büyük normal sapması [{0}]"));

SS_MSG(help_method,
    EN("opacity: the opacity field; tsdf: fused per-camera depth, as 2DGS meshes (needs cameras) [{0}]"),
    JA("opacity: 不透明度の場、tsdf: カメラごとの深度を統合（2DGS と同じ。カメラが必要）[{0}]"),
    ZH_HANS("opacity：不透明度场；tsdf：融合各相机的深度，同 2DGS（需要相机）[{0}]"),
    ZH_HANT("opacity：不透明度場；tsdf：融合各相機的深度，同 2DGS（需要相機）[{0}]"),
    KO("opacity: 불투명도 장, tsdf: 카메라별 깊이 융합, 2DGS와 같음(카메라 필요) [{0}]"),
    DE("opacity: das Opazitätsfeld; tsdf: fusionierte Tiefe je Kamera, wie bei 2DGS (braucht Kameras) [{0}]"),
    FR("opacity : le champ d'opacité ; tsdf : profondeur fusionnée par caméra, comme 2DGS (caméras requises) [{0}]"),
    ES("opacity: el campo de opacidad; tsdf: profundidad fusionada por cámara, como 2DGS (necesita cámaras) [{0}]"),
    PT("opacity: o campo de opacidade; tsdf: profundidade fundida por câmera, como no 2DGS (precisa de câmeras) [{0}]"),
    IT("opacity: il campo di opacità; tsdf: profondità fusa per camera, come 2DGS (servono le camere) [{0}]"),
    NL("opacity: het opaciteitsveld; tsdf: samengevoegde diepte per camera, zoals 2DGS (camera's nodig) [{0}]"),
    RU("opacity: поле непрозрачности; tsdf: слитая глубина по камерам, как в 2DGS (нужны камеры) [{0}]"),
    TR("opacity: opaklık alanı; tsdf: kamera başına birleştirilmiş derinlik, 2DGS'deki gibi (kamera gerekir) [{0}]"));

SS_MSG(help_tsdf_voxel,
    EN("TSDF voxel size of a uniform grid; 0 follows the Gaussians instead [{0}]"),
    JA("均一グリッドの TSDF ボクセルの大きさ。0 でガウシアンの密度に合わせます [{0}]"),
    ZH_HANS("均匀网格的 TSDF 体素大小；0 表示随高斯的疏密自适应 [{0}]"),
    ZH_HANT("均勻網格的 TSDF 體素大小；0 表示隨高斯的疏密自適應 [{0}]"),
    KO("균일 격자의 TSDF 복셀 크기. 0이면 가우시안 밀도를 따릅니다 [{0}]"),
    DE("TSDF-Voxelgröße eines gleichmäßigen Gitters; 0 folgt den Gaussians [{0}]"),
    FR("taille des voxels TSDF d'une grille uniforme ; 0 suit les gaussiennes [{0}]"),
    ES("tamaño del vóxel TSDF de una rejilla uniforme; 0 sigue a las gaussianas [{0}]"),
    PT("tamanho do voxel TSDF de uma grade uniforme; 0 segue as gaussianas [{0}]"),
    IT("dimensione dei voxel TSDF di una griglia uniforme; 0 segue le gaussiane [{0}]"),
    NL("TSDF-voxelgrootte van een uniform raster; 0 volgt de gaussians [{0}]"),
    RU("размер вокселя TSDF равномерной сетки; 0 — по плотности гауссиан [{0}]"),
    TR("düzgün ızgaranın TSDF voksel boyutu; 0 Gauss'ları izler [{0}]"));

SS_MSG(help_tsdf_trunc,
    EN("TSDF truncation band, in voxels; in pixels without a grid [{0}]"),
    JA("TSDF の打ち切り幅（ボクセル単位。グリッドなしではピクセル単位）[{0}]"),
    ZH_HANS("TSDF 截断带宽，以体素计；无网格时以像素计 [{0}]"),
    ZH_HANT("TSDF 截斷帶寬，以體素計；無網格時以像素計 [{0}]"),
    KO("TSDF 절단 폭(복셀 단위, 격자가 없으면 픽셀 단위) [{0}]"),
    DE("TSDF-Kappungsband in Voxeln; ohne Gitter in Pixeln [{0}]"),
    FR("bande de troncature TSDF, en voxels ; en pixels sans grille [{0}]"),
    ES("banda de truncamiento TSDF, en vóxeles; en píxeles sin rejilla [{0}]"),
    PT("faixa de truncamento TSDF, em voxels; em pixels sem grade [{0}]"),
    IT("banda di troncamento TSDF, in voxel; in pixel senza griglia [{0}]"),
    NL("TSDF-afkapband, in voxels; in pixels zonder raster [{0}]"),
    RU("полоса усечения TSDF, в вокселях; без сетки — в пикселях [{0}]"),
    TR("voksel cinsinden TSDF kesme bandı; ızgarasız piksel cinsinden [{0}]"));

SS_MSG(help_tsdf_max_depth,
    EN("ignore depth beyond this distance in scene units; 0 keeps all [{0}]"),
    JA("この距離（シーンの単位）より遠い深度を無視します。0 ですべて使います [{0}]"),
    ZH_HANS("忽略超过此距离（场景单位）的深度；0 表示全部保留 [{0}]"),
    ZH_HANT("忽略超過此距離（場景單位）的深度；0 表示全部保留 [{0}]"),
    KO("이 거리(장면 단위)보다 먼 깊이를 무시합니다. 0이면 모두 씁니다 [{0}]"),
    DE("Tiefe jenseits dieser Entfernung in Szeneneinheiten ignorieren; 0 behält alles [{0}]"),
    FR("ignorer la profondeur au-delà de cette distance, en unités de la scène ; 0 garde tout [{0}]"),
    ES("ignorar la profundidad más allá de esta distancia, en unidades de la escena; 0 lo conserva todo [{0}]"),
    PT("ignorar a profundidade além desta distância, em unidades da cena; 0 mantém tudo [{0}]"),
    IT("ignora la profondità oltre questa distanza, in unità della scena; 0 tiene tutto [{0}]"),
    NL("diepte voorbij deze afstand negeren, in scène-eenheden; 0 houdt alles [{0}]"),
    RU("не учитывать глубину дальше этого расстояния в единицах сцены; 0 — учитывать всё [{0}]"),
    TR("sahne biriminde bu uzaklığın ötesindeki derinliği yok say; 0 hepsini tutar [{0}]"));

SS_MSG(help_iso,
    EN("isosurface level [0.5 with cameras, 0.2 without]"),
    JA("等値面のしきい値 [カメラありで 0.5、なしで 0.2]"),
    ZH_HANS("等值面阈值 [有相机时 0.5，没有时 0.2]"),
    ZH_HANT("等值面閾值 [有相機時 0.5，沒有時 0.2]"),
    KO("등가면 값 [카메라가 있으면 0.5, 없으면 0.2]"),
    DE("Niveau der Isofläche [0.5 mit Kameras, 0.2 ohne]"),
    FR("niveau de l'isosurface [0.5 avec caméras, 0.2 sans]"),
    ES("nivel de la isosuperficie [0.5 con cámaras, 0.2 sin ellas]"),
    PT("nível da isossuperfície [0.5 com câmeras, 0.2 sem]"),
    IT("livello dell'isosuperficie [0.5 con le camere, 0.2 senza]"),
    NL("niveau van het isovlak [0.5 met camera's, 0.2 zonder]"),
    RU("уровень изоповерхности [0.5 с камерами, 0.2 без]"),
    TR("eş yüzey düzeyi [kameralarla 0.5, kamerasız 0.2]"));

SS_MSG(help_merge_factor,
    EN("scale on every merge limit; 0 turns merging off [{0}]"),
    JA("統合の各上限にかける倍率。0 で統合しない [{0}]"),
    ZH_HANS("所有合并上限的倍数；0 表示不合并 [{0}]"),
    ZH_HANT("所有合併上限的倍數；0 表示不合併 [{0}]"),
    KO("모든 병합 한도에 곱하는 배수. 0이면 병합하지 않습니다 [{0}]"),
    DE("Faktor für alle Merge-Grenzen; 0 schaltet das Zusammenführen ab [{0}]"),
    FR("facteur de toutes les limites de fusion ; 0 désactive la fusion [{0}]"),
    ES("factor de todos los límites de fusión; 0 desactiva la fusión [{0}]"),
    PT("fator de todos os limites de fusão; 0 desativa a fusão [{0}]"),
    IT("fattore di tutti i limiti di fusione; 0 disattiva la fusione [{0}]"),
    NL("factor op alle samenvoeggrenzen; 0 zet samenvoegen uit [{0}]"),
    RU("множитель всех пределов слияния; 0 отключает слияние [{0}]"),
    TR("tüm birleştirme sınırlarının çarpanı; 0 birleştirmeyi kapatır [{0}]"));

SS_MSG(help_merge_flat_px,
    EN("flatten bumps under this many pixels of the closest view [{0}]"),
    JA("最も近い視点でこのピクセル数未満の凹凸を平らにする [{0}]"),
    ZH_HANS("把最近视角下小于此像素数的起伏压平 [{0}]"),
    ZH_HANT("把最近視角下小於此像素數的起伏壓平 [{0}]"),
    KO("가장 가까운 시점에서 이 픽셀 수보다 작은 요철을 평평하게 합니다 [{0}]"),
    DE("Unebenheiten unter so vielen Pixeln der nächsten Ansicht glätten [{0}]"),
    FR("aplanir les bosses de moins de ce nombre de pixels de la vue la plus proche [{0}]"),
    ES("aplanar los relieves de menos de estos píxeles en la vista más cercana [{0}]"),
    PT("achatar relevos menores que estes pixels na vista mais próxima [{0}]"),
    IT("appiattire i rilievi sotto questi pixel della vista più vicina [{0}]"),
    NL("bulten kleiner dan zoveel pixels van het dichtste beeld vlakmaken [{0}]"),
    RU("сглаживать неровности меньше стольких пикселей ближайшего вида [{0}]"),
    TR("en yakın görünümde bu kadar pikselden küçük tümsekleri düzleştir [{0}]"));

SS_MSG(help_merge_thin,
    EN("thin parts move at most this share of their thickness [{0}]"),
    JA("薄い部分は厚みのこの割合までしか動かさない [{0}]"),
    ZH_HANS("薄的部分最多移动其厚度的这个比例 [{0}]"),
    ZH_HANT("薄的部分最多移動其厚度的這個比例 [{0}]"),
    KO("얇은 부분은 두께의 이 비율까지만 움직입니다 [{0}]"),
    DE("dünne Teile bewegen sich höchstens um diesen Anteil ihrer Dicke [{0}]"),
    FR("les parties fines bougent d'au plus cette part de leur épaisseur [{0}]"),
    ES("las partes finas se mueven como mucho esta fracción de su grosor [{0}]"),
    PT("as partes finas movem no máximo esta fração da espessura [{0}]"),
    IT("le parti sottili si spostano al massimo di questa quota dello spessore [{0}]"),
    NL("dunne delen schuiven hoogstens dit deel van hun dikte [{0}]"),
    RU("тонкие части сдвигаются не больше этой доли их толщины [{0}]"),
    TR("ince parçalar en fazla kalınlıklarının bu oranı kadar kayar [{0}]"));

SS_MSG(help_merge_max_edge_px,
    EN("longest edge a merge may leave, in pixels [{0}]"),
    JA("統合で残せる辺の最大長(ピクセル) [{0}]"),
    ZH_HANS("合并后允许的最长边（像素） [{0}]"),
    ZH_HANT("合併後允許的最長邊（像素） [{0}]"),
    KO("병합이 남길 수 있는 가장 긴 변(픽셀) [{0}]"),
    DE("längste Kante, die ein Merge hinterlassen darf, in Pixeln [{0}]"),
    FR("plus longue arête qu'une fusion peut laisser, en pixels [{0}]"),
    ES("arista más larga que puede dejar una fusión, en píxeles [{0}]"),
    PT("aresta mais longa que uma fusão pode deixar, em pixels [{0}]"),
    IT("spigolo più lungo che una fusione può lasciare, in pixel [{0}]"),
    NL("langste rand die samenvoegen mag achterlaten, in pixels [{0}]"),
    RU("самое длинное ребро после слияния, в пикселях [{0}]"),
    TR("birleştirmenin bırakabileceği en uzun kenar, piksel olarak [{0}]"));

SS_MSG(help_merge_color,
    EN("keep vertices whose merged colours differ by more than this, 0-1; 0 ignores colour [{0}]"),
    JA("統合する色の差がこれを超える頂点は残す(0〜1、0 で色を無視) [{0}]"),
    ZH_HANS("合并后颜色差超过此值的顶点保留（0–1，0 表示不看颜色） [{0}]"),
    ZH_HANT("合併後顏色差超過此值的頂點保留（0–1，0 表示不看顏色） [{0}]"),
    KO("합칠 색의 차이가 이보다 크면 정점을 남깁니다(0–1, 0이면 색 무시) [{0}]"),
    DE("Punkte behalten, deren zusammengeführte Farben stärker abweichen, 0–1; 0 ignoriert Farbe [{0}]"),
    FR("garder les sommets dont les couleurs fusionnées diffèrent plus que ceci, 0-1 ; 0 ignore la couleur [{0}]"),
    ES("conservar los vértices cuyos colores fusionados difieren más que esto, 0-1; 0 ignora el color [{0}]"),
    PT("manter os vértices cujas cores fundidas diferem mais que isto, 0-1; 0 ignora a cor [{0}]"),
    IT("mantenere i vertici i cui colori fusi differiscono più di così, 0-1; 0 ignora il colore [{0}]"),
    NL("punten houden waarvan de samengevoegde kleuren meer verschillen, 0-1; 0 negeert kleur [{0}]"),
    RU("сохранять вершины, чьи сливаемые цвета различаются сильнее, 0–1; 0 не учитывает цвет [{0}]"),
    TR("birleşen renkleri bundan fazla farklı olan köşeleri koru, 0-1; 0 rengi yok sayar [{0}]"));

SS_MSG(help_bisection_iters,
    EN("bisection steps per cut edge [{0}]"),
    JA("交差辺 1 本あたりの二分法の回数 [{0}]"),
    ZH_HANS("每条相交边的二分次数 [{0}]"),
    ZH_HANT("每條相交邊的二分次數 [{0}]"),
    KO("교차 모서리마다 이분법 횟수 [{0}]"),
    DE("Bisektionsschritte je Schnittkante [{0}]"),
    FR("étapes de bissection par arête coupée [{0}]"),
    ES("pasos de bisección por arista cortada [{0}]"),
    PT("passos de bisseção por aresta cortada [{0}]"),
    IT("passi di bisezione per spigolo tagliato [{0}]"),
    NL("bisectiestappen per snijrand [{0}]"),
    RU("шагов деления пополам на рассечённое ребро [{0}]"),
    TR("kesişen kenar başına ikiye bölme adımı [{0}]"));

SS_MSG(help_samples_per_gaussian,
    EN("samples per Gaussian: 7 (center + axes) or 1 (center only) [{0}]"),
    JA("ガウシアンごとの点の数: 7（中心＋軸）または 1（中心のみ）[{0}]"),
    ZH_HANS("每个高斯球的采样点数：7（中心＋轴）或 1（仅中心）[{0}]"),
    ZH_HANT("每個高斯球的取樣點數：7（中心＋軸）或 1（僅中心）[{0}]"),
    KO("가우스당 점 수: 7(중심+축) 또는 1(중심만) [{0}]"),
    DE("Punkte pro Gauß-Verteilung: 7 (Mitte + Achsen) oder 1 (nur Mitte) [{0}]"),
    FR("points par gaussienne : 7 (centre + axes) ou 1 (centre seul) [{0}]"),
    ES("puntos por gaussiana: 7 (centro + ejes) o 1 (solo centro) [{0}]"),
    PT("pontos por gaussiana: 7 (centro + eixos) ou 1 (apenas centro) [{0}]"),
    IT("punti per gaussiana: 7 (centro + assi) o 1 (solo centro) [{0}]"),
    NL("punten per Gaussian: 7 (midden + assen) of 1 (alleen midden) [{0}]"),
    RU("точек на гауссиану: 7 (центр + оси) или 1 (только центр) [{0}]"),
    TR("Gauss başına nokta: 7 (merkez + eksen) veya 1 (yalnızca merkez) [{0}]"));

SS_MSG(help_max_cameras,
    EN("cap on the cameras used, -1 for all of them [{0}]"),
    JA("使うカメラ数の上限。-1 ですべて [{0}]"),
    ZH_HANS("使用相机数的上限，-1 表示全部 [{0}]"),
    ZH_HANT("使用相機數的上限，-1 表示全部 [{0}]"),
    KO("쓰는 카메라 수 상한, -1 이면 전부 [{0}]"),
    DE("Obergrenze der verwendeten Kameras, -1 für alle [{0}]"),
    FR("plafond des caméras utilisées, -1 pour toutes [{0}]"),
    ES("tope de cámaras usadas, -1 para todas [{0}]"),
    PT("limite de câmeras usadas, -1 para todas [{0}]"),
    IT("tetto sulle camere usate, -1 per tutte [{0}]"),
    NL("plafond op gebruikte camera's, -1 voor alle [{0}]"),
    RU("предел числа используемых камер, -1 -- все [{0}]"),
    TR("kullanılan kamera üst sınırı, hepsi için -1 [{0}]"));

SS_MSG(help_max_grid_res,
    EN("acceleration grid cap [{0}]"),
    JA("加速用グリッドの上限 [{0}]"),
    ZH_HANS("加速网格的上限 [{0}]"),
    ZH_HANT("加速格點的上限 [{0}]"),
    KO("가속 격자의 상한 [{0}]"),
    DE("Obergrenze des Beschleunigungsgitters [{0}]"),
    FR("plafond de la grille d'accélération [{0}]"),
    ES("tope de la rejilla de aceleración [{0}]"),
    PT("limite da grade de aceleração [{0}]"),
    IT("tetto della griglia di accelerazione [{0}]"),
    NL("plafond van het versnellingsrooster [{0}]"),
    RU("предел ускоряющей сетки [{0}]"),
    TR("hızlandırma ızgarasının üst sınırı [{0}]"));

SS_MSG(help_grid_cell_factor,
    EN("grid cell size factor [{0}]"),
    JA("グリッドのセルサイズの係数 [{0}]"),
    ZH_HANS("网格单元尺寸的系数 [{0}]"),
    ZH_HANT("格點單元尺寸的係數 [{0}]"),
    KO("격자 칸 크기의 계수 [{0}]"),
    DE("Faktor der Gitterzellengröße [{0}]"),
    FR("facteur de taille des cellules de la grille [{0}]"),
    ES("factor del tamaño de celda de la rejilla [{0}]"),
    PT("fator do tamanho da célula da grade [{0}]"),
    IT("fattore della dimensione delle celle della griglia [{0}]"),
    NL("factor voor de celgrootte van het rooster [{0}]"),
    RU("коэффициент размера ячейки сетки [{0}]"),
    TR("ızgara hücre boyutu katsayısı [{0}]"));

SS_MSG(help_num_threads,
    EN("0 uses every hardware thread [{0}]"),
    JA("0 ならハードウェアスレッドをすべて使います [{0}]"),
    ZH_HANS("0 表示用上所有硬件线程 [{0}]"),
    ZH_HANT("0 表示用上所有硬體執行緒 [{0}]"),
    KO("0 이면 하드웨어 스레드를 모두 씁니다 [{0}]"),
    DE("0 nutzt jeden Hardware-Thread [{0}]"),
    FR("0 utilise tous les threads matériels [{0}]"),
    ES("0 usa todos los hilos de hardware [{0}]"),
    PT("0 usa todos os threads de hardware [{0}]"),
    IT("0 usa tutti i thread hardware [{0}]"),
    NL("0 gebruikt elke hardwarethread [{0}]"),
    RU("0 задействует все аппаратные потоки [{0}]"),
    TR("0, tüm donanım iş parçacıklarını kullanır [{0}]"));

SS_MSG(help_cull_unseen,
    EN("drop mesh vertices no camera saw [{0}]"),
    JA("どのカメラにも映らなかった頂点を落とす [{0}]"),
    ZH_HANS("丢弃没有任何相机看到的顶点 [{0}]"),
    ZH_HANT("丟棄沒有任何相機看到的頂點 [{0}]"),
    KO("어느 카메라도 보지 못한 정점을 버립니다 [{0}]"),
    DE("Ecken verwerfen, die keine Kamera gesehen hat [{0}]"),
    FR("écarter les sommets qu'aucune caméra n'a vus [{0}]"),
    ES("descartar los vértices que ninguna cámara vio [{0}]"),
    PT("descartar os vértices que nenhuma câmera viu [{0}]"),
    IT("scartare i vertici che nessuna camera ha visto [{0}]"),
    NL("hoekpunten weglaten die geen camera zag [{0}]"),
    RU("отбрасывать вершины, которых не видела ни одна камера [{0}]"),
    TR("hiçbir kameranın görmediği köşeleri at [{0}]"));

SS_MSG(help_merge_max_flip,
    EN("fold guard for merge collapses [{0}]"),
    JA("統合時の折り返しを防ぐ上限 [{0}]"),
    ZH_HANS("合并坍缩时防止折叠的上限 [{0}]"),
    ZH_HANT("合併塌縮時防止折疊的上限 [{0}]"),
    KO("병합 축약에서 접힘을 막는 상한 [{0}]"),
    DE("Faltungsschutz für Merge-Kollapse [{0}]"),
    FR("garde-fou de repli pour les effondrements de fusion [{0}]"),
    ES("resguardo contra pliegues en los colapsos de fusión [{0}]"),
    PT("proteção contra dobras nos colapsos de fusão [{0}]"),
    IT("protezione dalle pieghe nei collassi di fusione [{0}]"),
    NL("vouwbeveiliging voor samenvoeg-inklappingen [{0}]"),
    RU("защита от складок при схлопывании в слиянии [{0}]"),
    TR("birleştirme çökertmelerinde katlanma koruması [{0}]"));

SS_MSG(help_floater_min_faces,
    EN("drop components smaller than this [{0}]"),
    JA("これより小さい連結成分を落とす [{0}]"),
    ZH_HANS("丢弃小于此值的连通块 [{0}]"),
    ZH_HANT("丟棄小於此值的連通塊 [{0}]"),
    KO("이보다 작은 연결 성분을 버립니다 [{0}]"),
    DE("Komponenten unter dieser Größe verwerfen [{0}]"),
    FR("écarter les composantes plus petites que cela [{0}]"),
    ES("descartar las componentes menores que esto [{0}]"),
    PT("descartar os componentes menores que isto [{0}]"),
    IT("scartare le componenti più piccole di questo [{0}]"),
    NL("componenten kleiner dan dit weglaten [{0}]"),
    RU("отбрасывать компоненты меньше этого [{0}]"),
    TR("bundan küçük bileşenleri at [{0}]"));

SS_MSG(help_fill_hole_ratio,
    EN("fill loops smaller than this fraction of their component [{0}]"),
    JA("連結成分に対するこの割合より小さい輪を埋める [{0}]"),
    ZH_HANS("填补小于所在连通块此比例的边界环 [{0}]"),
    ZH_HANT("填補小於所在連通塊此比例的邊界環 [{0}]"),
    KO("연결 성분의 이 비율보다 작은 고리를 메웁니다 [{0}]"),
    DE("Schleifen unter diesem Anteil ihrer Komponente füllen [{0}]"),
    FR("combler les boucles sous cette fraction de leur composante [{0}]"),
    ES("rellenar los bucles menores que esta fracción de su componente [{0}]"),
    PT("preencher os laços menores que esta fração do seu componente [{0}]"),
    IT("chiudere gli anelli sotto questa frazione della loro componente [{0}]"),
    NL("lussen kleiner dan dit deel van hun component vullen [{0}]"),
    RU("залатывать петли меньше этой доли своей компоненты [{0}]"),
    TR("bileşeninin bu oranından küçük halkaları kapat [{0}]"));

SS_MSG(help_fill_hole_max_edges,
    EN("always fill loops with at most this many edges [{0}]"),
    JA("辺の数がこれ以下の輪は必ず埋める [{0}]"),
    ZH_HANS("边数不超过此值的环一律填补 [{0}]"),
    ZH_HANT("邊數不超過此值的環一律填補 [{0}]"),
    KO("모서리 수가 이 값 이하인 고리는 항상 메웁니다 [{0}]"),
    DE("Schleifen mit höchstens so vielen Kanten immer füllen [{0}]"),
    FR("toujours combler les boucles d'au plus tant d'arêtes [{0}]"),
    ES("rellenar siempre los bucles con como mucho estas aristas [{0}]"),
    PT("preencher sempre os laços com no máximo estas arestas [{0}]"),
    IT("chiudere sempre gli anelli con al massimo questi spigoli [{0}]"),
    NL("lussen met hoogstens zoveel randen altijd vullen [{0}]"),
    RU("всегда залатывать петли не длиннее стольких рёбер [{0}]"),
    TR("en çok bu kadar kenarı olan halkaları her zaman kapat [{0}]"));

SS_MSG(help_degenerate_angle,
    EN("repair triangles with angles below this [{0}]"),
    JA("これより小さい角を持つ三角形を直す [{0}]"),
    ZH_HANS("修复角度小于此值的三角形 [{0}]"),
    ZH_HANT("修復角度小於此值的三角形 [{0}]"),
    KO("이보다 작은 각을 가진 삼각형을 고칩니다 [{0}]"),
    DE("Dreiecke mit Winkeln darunter reparieren [{0}]"),
    FR("réparer les triangles dont les angles sont sous ce seuil [{0}]"),
    ES("reparar los triángulos con ángulos por debajo de esto [{0}]"),
    PT("reparar os triângulos com ângulos abaixo disto [{0}]"),
    IT("riparare i triangoli con angoli sotto questo [{0}]"),
    NL("driehoeken met hoeken hieronder herstellen [{0}]"),
    RU("чинить треугольники с углами меньше этого [{0}]"),
    TR("açıları bunun altındaki üçgenleri onar [{0}]"));

SS_MSG(help_quality_iters,
    EN("valence-flip and tangential-relaxation iterations [{0}]"),
    JA("価数の反転と接線方向の緩和の反復回数 [{0}]"),
    ZH_HANS("价数翻转与切向松弛的迭代次数 [{0}]"),
    ZH_HANT("價數翻轉與切向鬆弛的疊代次數 [{0}]"),
    KO("차수 뒤집기와 접선 방향 완화의 반복 횟수 [{0}]"),
    DE("Iterationen aus Valenz-Umklappung und tangentialer Relaxation [{0}]"),
    FR("itérations de bascule de valence et de relaxation tangentielle [{0}]"),
    ES("iteraciones de volteo de valencia y relajación tangencial [{0}]"),
    PT("iterações de inversão de valência e relaxamento tangencial [{0}]"),
    IT("iterazioni di ribaltamento di valenza e rilassamento tangenziale [{0}]"),
    NL("iteraties van valentie-omklap en tangentiële relaxatie [{0}]"),
    RU("итерации перекладывания по валентности и касательной релаксации [{0}]"),
    TR("değerlik çevirme ve teğetsel gevşetme yinelemeleri [{0}]"));

SS_MSG(help_check,
    EN("check the mesh for defects and exit 1 on any: 1 the result, 2 also after every stage; a mesh file is only checked [{0}]"),
    JA("メッシュの欠陥を検査し、あれば終了コード 1: 1 は結果、2 は各段階の後も。メッシュファイルは検査のみ [{0}]"),
    ZH_HANS("检查网格缺陷，有则以 1 退出：1 检查结果，2 每个阶段后也检查；网格文件只检查 [{0}]"),
    ZH_HANT("檢查網格缺陷，有則以 1 結束：1 檢查結果，2 每個階段後也檢查；網格檔只檢查 [{0}]"),
    KO("메시 결함을 검사하고 있으면 1로 종료: 1은 결과, 2는 단계마다도; 메시 파일은 검사만 [{0}]"),
    DE("Netz auf Fehler prüfen, bei einem mit 1 beenden: 1 das Ergebnis, 2 auch nach jeder Stufe; eine Netzdatei wird nur geprüft [{0}]"),
    FR("vérifier les défauts du maillage, quitter avec 1 s'il y en a : 1 le résultat, 2 aussi après chaque étape ; un fichier de maillage est seulement vérifié [{0}]"),
    ES("revisar defectos de la malla y salir con 1 si hay: 1 el resultado, 2 también tras cada etapa; un archivo de malla solo se revisa [{0}]"),
    PT("verificar defeitos da malha e sair com 1 se houver: 1 o resultado, 2 também após cada etapa; um ficheiro de malha só é verificado [{0}]"),
    IT("controllare i difetti della mesh e uscire con 1 se ce ne sono: 1 il risultato, 2 anche dopo ogni fase; un file mesh viene solo controllato [{0}]"),
    NL("mesh op gebreken controleren en met 1 stoppen als er zijn: 1 het resultaat, 2 ook na elke stap; een meshbestand wordt alleen gecontroleerd [{0}]"),
    RU("проверить меш на дефекты и выйти с кодом 1, если есть: 1 результат, 2 ещё и после каждого этапа; файл меша только проверяется [{0}]"),
    TR("ağı kusurlara karşı denetle, varsa 1 ile çık: 1 sonuç, 2 her aşamadan sonra da; ağ dosyası yalnızca denetlenir [{0}]"));

SS_MSG(help_progress_dir,
    EN("write how far the run is and the memory it expects into <dir>/memory.json while it goes, for a front end that is watching"),
    JA("実行中、進み具合と見込みのメモリ量を <dir>/memory.json に書き出す（見守るフロントエンド向け）"),
    ZH_HANS("运行期间把进度和预计的内存写入 <dir>/memory.json，供监视它的前端读取"),
    ZH_HANT("執行期間把進度和預計的記憶體寫入 <dir>/memory.json，供監看它的前端讀取"),
    KO("실행 중 진행 정도와 예상 메모리를 <dir>/memory.json에 기록합니다(지켜보는 프런트엔드용)"),
    DE("schreibt während des Laufs Fortschritt und erwarteten Speicher nach <dir>/memory.json, für eine Oberfläche, die zusieht"),
    FR("écrit pendant le calcul l'avancement et la mémoire prévue dans <dir>/memory.json, pour une interface qui le suit"),
    ES("escribe durante la ejecución el avance y la memoria prevista en <dir>/memory.json, para una interfaz que la observa"),
    PT("grava durante a execução o progresso e a memória prevista em <dir>/memory.json, para uma interface que a acompanha"),
    IT("scrive durante l'esecuzione l'avanzamento e la memoria prevista in <dir>/memory.json, per un'interfaccia che la segue"),
    NL("schrijft tijdens het draaien de voortgang en het verwachte geheugen naar <dir>/memory.json, voor een interface die meekijkt"),
    RU("во время работы записывает ход выполнения и ожидаемую память в <dir>/memory.json для следящего интерфейса"),
    TR("çalışırken ilerlemeyi ve beklenen belleği, izleyen bir arayüz için <dir>/memory.json dosyasına yazar"));

SS_MSG(help_quiet,
    EN("less progress output"),
    JA("進捗の出力を減らす"),
    ZH_HANS("减少进度输出"),
    ZH_HANT("減少進度輸出"),
    KO("진행 상황 출력을 줄입니다"),
    DE("weniger Fortschrittsausgabe"),
    FR("moins de sortie de progression"),
    ES("menos mensajes de progreso"),
    PT("menos mensagens de progresso"),
    IT("meno output di avanzamento"),
    NL("minder voortgangsuitvoer"),
    RU("меньше сообщений о ходе работы"),
    TR("daha az ilerleme çıktısı"));

}  // namespace mesh
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
