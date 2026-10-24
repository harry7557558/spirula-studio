#pragma once

// What `spirula marker` says -- its --help and progress lines.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace marker {

SS_MSG(tagline,
    EN("Detect fiducial markers (AprilTag) to align coordinates, scale and crop"),
    JA("基準マーカー (AprilTag) を検出して座標系、スケール、切り抜きを設定"),
    ZH_HANS("检测基准标记 (AprilTag) 以对齐坐标系、缩放尺度和裁剪"),
    ZH_HANT("偵測基準標記 (AprilTag) 以對齊座標系、縮放尺度和裁剪"),
    KO("기준 마커(AprilTag)를 감지하여 좌표계, 스케일 및 자르기 설정"),
    DE("Referenzmarken (AprilTag) erkennen zum Ausrichten, Skalieren und Zuschneiden"),
    FR("Détecter les marqueurs fiduciaires (AprilTag) pour aligner, mettre à l'échelle et recadrer"),
    ES("Detectar marcadores fiduciarios (AprilTag) para alinear coordenadas, escalar y recortar"),
    PT("Detectar marcadores fiduciários (AprilTag) para alinhar coordenadas, dimensionar e recortar"),
    IT("Rileva marcatori fiduciari (AprilTag) per allineare le coordinate, scalare e ritagliare"),
    NL("Detecteer fiduciale markeringen (AprilTag) om coördinaten uit te lijnen, te schalen en bij te snijden"),
    RU("Обнаружение опорных меток (AprilTag) для выравнивания координат, масштаба и обрезки"),
    TR("Koordinatları hizalamak, ölçeklemek ve kırpmak için işaretçileri (AprilTag) algıla"));

SS_MSG(usage_about,
    EN("Detects fiducial markers (tag36h10 / tag36h11) in dataset photos, triangulates them into 3D,\nderives a Sim(3) similarity to establish a metric coordinate system from 3 markers or a GCP table,\nand generates an automatic crop box (BoxRegion) in <dataset>/roi/."),
    JA("写真内の基準マーカー (tag36h10 / tag36h11) を検出し、3D 三角測量を行い、3 つのマーカーまたは GCP 表から実スケールの座標系 (Sim(3)) を求めて、自動切り抜きボックス (BoxRegion) を <dataset>/roi/ に生成します。"),
    ZH_HANS("检测照片中的基准标记 (tag36h10 / tag36h11)，在 3D 空间进行三角测量，利用 3 个标记或 GCP 建立公制坐标系 (Sim(3))，并在 <dataset>/roi/ 中自动生成裁剪框 (BoxRegion)。"),
    ZH_HANT("偵測照片中的基準標記 (tag36h10 / tag36h11)，在 3D 空間進行三角測量，利用 3 個標記或 GCP 建立公制座標系 (Sim(3))，並在 <dataset>/roi/ 中自動產生裁剪框 (BoxRegion)。"),
    KO("사진에서 기준 마커(tag36h10 / tag36h11)를 감지하고 3D로 삼각측량하여 3개의 마커 또는 GCP 표로부터 실제 미터 좌표계(Sim(3))를 설정하고 <dataset>/roi/에 자동 자르기 상자(BoxRegion)를 생성합니다."),
    DE("Erkennt Referenzmarken (tag36h10 / tag36h11) in Fotos, trianguliert sie in 3D, bestimmt eine Sim(3)-Ähnlichkeitstransformation für ein metrisches Koordinatensystem und erzeugt einen automatischen Begrenzungsrahmen (BoxRegion) in <dataset>/roi/."),
    FR("Détecte les marqueurs fiduciaires (tag36h10 / tag36h11) dans les photos, les triangule en 3D, dérive une similarité Sim(3) pour établir un repère métrique et génère une boîte englobante (BoxRegion) dans <dataset>/roi/."),
    ES("Detecta marcadores fiduciarios (tag36h10 / tag36h11) en fotos, los triangula en 3D, deriva una similitud Sim(3) para establecer un sistema de coordenadas métrico y genera un cuadro delimitador (BoxRegion) en <dataset>/roi/."),
    PT("Detecta marcadores fiduciários (tag36h10 / tag36h11) em fotos, triangula-os em 3D, deriva uma similaridade Sim(3) para estabelecer um sistema de coordenadas métrico e gera uma caixa delimitadora (BoxRegion) em <dataset>/roi/."),
    IT("Rileva i marcatori fiduciari (tag36h10 / tag36h11) nelle foto, li triangola in 3D, calcola una similitudine Sim(3) per stabilire un sistema metrico e genera un riquadro di ritaglio (BoxRegion) in <dataset>/roi/."),
    NL("Detecteert fiduciale markeringen (tag36h10 / tag36h11) in foto's, trianguleert ze naar 3D, leidt een Sim(3)-transformatie af voor een metrisch coördinatensysteem en genereert een automatisch begrenzingskader (BoxRegion) in <dataset>/roi/."),
    RU("Обнаруживает метки (tag36h10 / tag36h11) на фото, триангулирует их в 3D, вычисляет подобие Sim(3) для метрической системы координат и создает ограничивающий объем (BoxRegion) в <dataset>/roi/."),
    TR("Fotoğraflardaki işaretçileri (tag36h10 / tag36h11) algılar, 3B'de nirengiler, metrik koordinat sistemi için Sim(3) benzerliğini çıkarır ve <dataset>/roi/ içinde otomatik kırpma kutusu (BoxRegion) oluşturur."));

SS_MSG(detecting_markers,
    EN("Detecting markers in {0} images..."),
    JA("{0} 枚の画像からマーカーを検出中..."),
    ZH_HANS("正在从 {0} 张图像中检测标记..."),
    ZH_HANT("正在從 {0} 張影像中偵測標記..."),
    KO("{0}개 이미지에서 마커 감지 중..."),
    DE("Marken in {0} Bildern werden erkannt..."),
    FR("Détection des marqueurs dans {0} images..."),
    ES("Detectando marcadores en {0} imágenes..."),
    PT("Detectando marcadores em {0} imagens..."),
    IT("Rilevamento dei marcatori in {0} immagini..."),
    NL("Markeringen detecteren in {0} afbeeldingen..."),
    RU("Поиск меток на {0} изображениях..."),
    TR("{0} görüntüde işaretçiler algılanıyor..."));

SS_MSG(detected_summary,
    EN("Detected {0} marker observations across {1} images ({2} distinct tags)"),
    JA("{1} 枚の画像から {0} 件のマーカー観測を検出（{2} 種類のタグ）"),
    ZH_HANS("在 {1} 张图像中检测到 {0} 次标记观测（共 {2} 个不同标记）"),
    ZH_HANT("在 {1} 張影像中偵測到 {0} 次標記觀測（共 {2} 個不同標記）"),
    KO("{1}개 이미지에서 {0}개의 마커 관측 감지({2}개의 고유 태그)"),
    DE("{0} Markierungsbeobachtungen in {1} Bildern erkannt ({2} verschiedene Tags)"),
    FR("{0} observations de marqueurs détectées sur {1} images ({2} tags distincts)"),
    ES("Se detectaron {0} observaciones de marcadores en {1} imágenes ({2} etiquetas distintas)"),
    PT("{0} observações de marcadores detectadas em {1} imagens ({2} tags distintas)"),
    IT("Rilevate {0} osservazioni di marcatori in {1} immagini ({2} tag distinti)"),
    NL("{0} markeringswaarnemingen gedetecteerd over {1} afbeeldingen ({2} unieke tags)"),
    RU("Обнаружено {0} наблюдений меток на {1} изображениях ({2} уникальных меток)"),
    TR("{1} görüntüde {0} işaretçi gözlemi algılandı ({2} farklı etiket)"));

SS_MSG(triangulated_summary,
    EN("Triangulated {0} markers in 3D reconstruction space"),
    JA("3D 再構成空間内で {0} 個のマーカーを三角測量しました"),
    ZH_HANS("在 3D 重建空间中成功三角测量了 {0} 个标记"),
    ZH_HANT("在 3D 重建空間中成功三角測量了 {0} 個標記"),
    KO("3D 재구성 공간에서 {0}개의 마커를 삼각측량했습니다"),
    DE("{0} Marken im 3D-Rekonstruktionsraum trianguliert"),
    FR("{0} marqueurs triangulés dans l'espace de reconstruction 3D"),
    ES("{0} marcadores triangulados en el espacio de reconstrucción 3D"),
    PT("{0} marcadores triangulados no espaço de reconstrução 3D"),
    IT("Triangolati {0} marcatori nello spazio di ricostruzione 3D"),
    NL("{0} markeringen getrianguleerd in de 3D-reconstructieruimte"),
    RU("Триангулировано {0} меток в пространстве 3D-реконструкции"),
    TR("3B rekonstrüksiyon alanında {0} işaretçi nirengilendi"));

SS_MSG(aligning_system,
    EN("Solving coordinate system from markers (origin: {0}, x-axis: {1}, xy-plane: {2})..."),
    JA("マーカーから座標系を計算中（原点: {0}、X 軸: {1}、XY 平面: {2}）..."),
    ZH_HANS("正在根据标记计算坐标系（原点: {0}，X 轴: {1}，XY 平面: {2}）..."),
    ZH_HANT("正在根據標記計算座標系（原點: {0}，X 軸: {1}，XY 平面: {2}）..."),
    KO("마커로부터 좌표계 계산 중(원점: {0}, X 축: {1}, XY 평면: {2})..."),
    DE("Koordinatensystem aus Marken wird berechnet (Ursprung: {0}, X-Achse: {1}, XY-Ebene: {2})..."),
    FR("Calcul du système de coordonnées à partir des marqueurs (origine : {0}, axe X : {1}, plan XY : {2})..."),
    ES("Calculando el sistema de coordenadas a partir de marcadores (origen: {0}, eje X: {1}, plano XY: {2})..."),
    PT("Calculando o sistema de coordenadas a partir dos marcadores (origem: {0}, eixo X: {1}, plano XY: {2})..."),
    IT("Calcolo del sistema di coordinate dai marcatori (origine: {0}, asse X: {1}, piano XY: {2})..."),
    NL("Coördinatensysteem berekenen uit markeringen (oorsprong: {0}, X-as: {1}, XY-vlak: {2})..."),
    RU("Расчет системы координат по меткам (начало: {0}, ось X: {1}, плоскость XY: {2})..."),
    TR("İşaretçilerden koordinat sistemi çözülüyor (başlangıç: {0}, X ekseni: {1}, XY düzlemi: {2})..."));

SS_MSG(align_success,
    EN("Alignment solved: scale={0}, translation=({1}, {2}, {3})"),
    JA("整列完了: スケール={0}、平行移動=({1}, {2}, {3})"),
    ZH_HANS("对齐求解完成: 缩放={0}，平移=({1}, {2}, {3})"),
    ZH_HANT("對齊求解完成: 縮放={0}，平移=({1}, {2}, {3})"),
    KO("정렬 계산 완료: 스케일={0}, 이동=({1}, {2}, {3})"),
    DE("Ausrichtung berechnet: Skalierung={0}, Translation=({1}, {2}, {3})"),
    FR("Alignement résolu : échelle={0}, translation=({1}, {2}, {3})"),
    ES("Alineación resuelta: escala={0}, traslación=({1}, {2}, {3})"),
    PT("Alinhamento resolvido: escala={0}, translação=({1}, {2}, {3})"),
    IT("Allineamento risolto: scala={0}, traslazione=({1}, {2}, {3})"),
    NL("Uitlijning berekend: schaal={0}, translatie=({1}, {2}, {3})"),
    RU("Выравнивание рассчитано: масштаб={0}, сдвиг=({1}, {2}, {3})"),
    TR("Hizalama çözüldü: ölçek={0}, öteleme=({1}, {2}, {3})"));

SS_MSG(crop_success,
    EN("Generated automatic crop box saved to {0}"),
    JA("生成された自動切り抜きボックスを {0} に保存しました"),
    ZH_HANS("已将生成的自动裁剪框保存至 {0}"),
    ZH_HANT("已將產生的自動裁剪框儲存至 {0}"),
    KO("생성된 자동 자르기 상자를 {0}에 저장했습니다"),
    DE("Automatischer Begrenzungsrahmen gespeichert unter {0}"),
    FR("Boîte de découpe automatique enregistrée sous {0}"),
    ES("Caja de recorte automático guardada en {0}"),
    PT("Caixa de corte automático salva em {0}"),
    IT("Riquadro di ritaglio automatico salvato in {0}"),
    NL("Automatisch begrenzingskader opgeslagen in {0}"),
    RU("Автоматический ограничивающий объем сохранен в {0}"),
    TR("Oluşturulan otomatik kırpma kutusu {0} konumuna kaydedildi"));

SS_MSG(apply_splat_success,
    EN("Aligned splat model saved to {0}"),
    JA("整列されたスプラットモデルを {0} に保存しました"),
    ZH_HANS("已将对齐后的高斯模型保存至 {0}"),
    ZH_HANT("已將對齊後的高斯模型儲存至 {0}"),
    KO("정렬된 스플랫 모델을 {0}에 저장했습니다"),
    DE("Ausgerichtetes Splat-Modell gespeichert unter {0}"),
    FR("Modèle de splats aligné enregistré sous {0}"),
    ES("Modelo de splats alineado guardado en {0}"),
    PT("Modelo de splats alinhado salvo em {0}"),
    IT("Modello di splat allineato salvato in {0}"),
    NL("Uitgelijnd splat-model opgeslagen in {0}"),
    RU("Выровненная модель сплэтов сохранена в {0}"),
    TR("Hizalanmış splat modeli {0} konumuna kaydedildi"));

SS_MSG(err_no_markers,
    EN("No valid markers detected or triangulated with >= 2 views"),
    JA("2 視点以上で検出または三角測量された有効なマーカーがありません"),
    ZH_HANS("未检测到或未能在至少 2 个视角中三角测量出有效标记"),
    ZH_HANT("未偵測到或未能在至少 2 個視角中三角測量出有效標記"),
    KO("2개 이상의 뷰에서 유효한 마커가 감지되거나 삼각측량되지 않았습니다"),
    DE("Keine gültigen Marken mit mindestens 2 Ansichten erkannt oder trianguliert"),
    FR("Aucun marqueur valide détecté ou triangulé sur au moins 2 vues"),
    ES("No se detectaron ni triangularon marcadores válidos con >= 2 vistas"),
    PT("Nenhum marcador válido detectado ou triangulado com >= 2 vistas"),
    IT("Nessun marcatore valido rilevato o triangolato con almeno 2 viste"),
    NL("Geen geldige markeringen gedetecteerd of getrianguleerd met >= 2 aanzichten"),
    RU("Не обнаружено валидных меток с >= 2 ракурсами"),
    TR("En az 2 görünümde geçerli işaretçi algılanamadı veya nirengilenemedi"));

SS_MSG(err_solve_failed,
    EN("Failed to solve marker coordinate system (markers collinear or missing)"),
    JA("マーカー座標系の計算に失敗しました（マーカーが同一直線上にあるか見つかりません）"),
    ZH_HANS("计算标记坐标系失败（标记共线或缺失）"),
    ZH_HANT("計算標記座標系失敗（標記共線或缺失）"),
    KO("마커 좌표계 계산 실패(마커가 일직선상에 있거나 누락됨)"),
    DE("Berechnung des Markenkoordinatensystems fehlgeschlagen (Marken kollinear oder fehlen)"),
    FR("Échec du calcul du repère des marqueurs (marqueurs colinéaires ou manquants)"),
    ES("Error al calcular el sistema de coordenadas (marcadores colineales o faltantes)"),
    PT("Falha ao calcular o sistema de coordenadas (marcadores colineares ou ausentes)"),
    IT("Impossibile calcolare il sistema di coordinate (marcatori collineari o mancanti)"),
    NL("Kan het markeringscoördinatensysteem niet berekenen (markeringen liggen op één lijn of ontbreken)"),
    RU("Не удалось рассчитать систему координат (метки коллинеарны или отсутствуют)"),
    TR("İşaretçi koordinat sistemi çözülemedi (işaretçiler doğrusal veya eksik)"));

SS_MSG(err_dataset_invalid,
    EN("Invalid dataset directory: {0}"),
    JA("無効なデータセットディレクトリ: {0}"),
    ZH_HANS("无效的数据集目录: {0}"),
    ZH_HANT("無效的資料集目錄: {0}"),
    KO("유효하지 않은 데이터셋 디렉터리: {0}"),
    DE("Ungültiges Datensatzverzeichnis: {0}"),
    FR("Répertoire de dataset invalide : {0}"),
    ES("Directorio de dataset no válido: {0}"),
    PT("Diretório de dataset inválido: {0}"),
    IT("Directory del dataset non valida: {0}"),
    NL("Ongeldige datasetmap: {0}"),
    RU("Недопустимый каталог датасета: {0}"),
    TR("Geçersiz veri seti dizini: {0}"));

}  // namespace marker
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
