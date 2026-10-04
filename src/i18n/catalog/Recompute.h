#pragma once

// "Recompute Sparse Point Cloud" on the training screen (app/gui/RecomputePanel.h).
// Folder names and `--poses` stay as they are in every language.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace recompute {

SS_MSG(button,
    EN("Recompute Sparse Pointcloud"), JA("スパース点群を再計算"),
    ZH_HANS("重新计算稀疏点云"), ZH_HANT("重新計算稀疏點雲"), KO("희소 포인트 클라우드 재계산"),
    DE("Dünne Punktwolke neu berechnen"), FR("Recalculer le nuage de points épars"),
    ES("Recalcular la nube de puntos dispersa"), PT("Recalcular a nuvem de pontos esparsa"),
    IT("Ricalcola la nuvola di punti sparsa"), NL("Dunne puntenwolk opnieuw berekenen"),
    RU("Пересчитать разреженное облако точек"), TR("Seyrek nokta bulutunu yeniden hesapla"));

SS_MSG(button_help,
    EN("New points from the images for the cameras this dataset already has, such as a "
       "matchmove solve that came with only its tracker points. The cameras and poses "
       "stay exactly as they are."),
    JA("このデータセットにすでにあるカメラに対して、画像から新しい点を求めます。"
       "トラッカーの点しか付いていないマッチムーブの解などに使います。カメラと姿勢は"
       "そのまま変わりません。"),
    ZH_HANS("为本数据集已有的相机从图像中求出新的点，例如只带跟踪点的匹配移动解算。"
            "相机和位姿完全保持不变。"),
    ZH_HANT("為本資料集已有的相機從影像中求出新的點，例如只帶追蹤點的匹配移動解算。"
            "相機和位姿完全保持不變。"),
    KO("이 데이터셋에 이미 있는 카메라에 대해 이미지에서 새 점을 구합니다. 트래커 점만 "
       "딸려 온 매치무브 솔브 같은 경우에 씁니다. 카메라와 포즈는 그대로 유지됩니다."),
    DE("Neue Punkte aus den Bildern für die Kameras, die dieser Datensatz schon hat, etwa "
       "ein Matchmove-Solve, der nur seine Trackerpunkte mitbringt. Kameras und Posen "
       "bleiben genau, wie sie sind."),
    FR("De nouveaux points tirés des images pour les caméras que ce jeu de données a "
       "déjà, par exemple un solve de matchmove livré avec ses seuls points de suivi. "
       "Les caméras et leurs poses restent exactement telles quelles."),
    ES("Puntos nuevos a partir de las imágenes para las cámaras que este conjunto de "
       "datos ya tiene, como un solve de matchmove que llegó solo con sus puntos de "
       "seguimiento. Las cámaras y sus poses quedan exactamente como están."),
    PT("Novos pontos a partir das imagens para as câmeras que este conjunto de dados já "
       "tem, como um solve de matchmove que veio só com os pontos de rastreamento. As "
       "câmeras e as poses ficam exatamente como estão."),
    IT("Nuovi punti dalle immagini per le fotocamere che questo dataset ha già, come un "
       "solve di matchmove arrivato con i soli punti di tracking. Fotocamere e pose "
       "restano esattamente come sono."),
    NL("Nieuwe punten uit de beelden voor de camera's die deze dataset al heeft, zoals "
       "een matchmove-solve die alleen zijn trackerpunten meebracht. De camera's en hun "
       "standen blijven precies zoals ze zijn."),
    RU("Новые точки по изображениям для камер, которые в наборе данных уже есть, "
       "например для решения матчмува, пришедшего только с точками трекеров. Камеры и "
       "их позы остаются в точности такими же."),
    TR("Bu veri kümesinde zaten olan kameralar için görüntülerden yeni noktalar; örneğin "
       "yalnızca izleyici noktalarıyla gelen bir matchmove çözümü. Kameralar ve pozları "
       "olduğu gibi kalır."));

SS_MSG(no_model,
    EN("Needs a COLMAP model in this dataset, with cameras.bin and images.bin."),
    JA("このデータセットに cameras.bin と images.bin を含む COLMAP モデルが必要です。"),
    ZH_HANS("需要本数据集中有包含 cameras.bin 和 images.bin 的 COLMAP 模型。"),
    ZH_HANT("需要本資料集中有包含 cameras.bin 和 images.bin 的 COLMAP 模型。"),
    KO("이 데이터셋에 cameras.bin과 images.bin이 있는 COLMAP 모델이 필요합니다."),
    DE("Braucht ein COLMAP-Modell in diesem Datensatz, mit cameras.bin und images.bin."),
    FR("Il faut un modèle COLMAP dans ce jeu de données, avec cameras.bin et images.bin."),
    ES("Necesita un modelo COLMAP en este conjunto de datos, con cameras.bin e images.bin."),
    PT("Precisa de um modelo COLMAP neste conjunto de dados, com cameras.bin e images.bin."),
    IT("Serve un modello COLMAP in questo dataset, con cameras.bin e images.bin."),
    NL("Vereist een COLMAP-model in deze dataset, met cameras.bin en images.bin."),
    RU("Нужна модель COLMAP в этом наборе данных, с cameras.bin и images.bin."),
    TR("Bu veri kümesinde cameras.bin ve images.bin içeren bir COLMAP modeli gerekir."));

// {0} the model whose cameras are kept.
SS_MSG(explain,
    EN("Keeps every camera in {0} exactly as it is and finds new points by matching the "
       "images. They replace the model's points3D.bin, and images.bin is updated to match "
       "with its poses unchanged; the previous files are kept as points3D.bin_original and "
       "images.bin_original."),
    JA("{0} のカメラをすべてそのまま保ち、画像どうしを照合して新しい点を求めます。"
       "新しい点はモデルの points3D.bin を置き換え、images.bin も姿勢はそのままで合わせて"
       "更新されます。以前のファイルは points3D.bin_original と images.bin_original として"
       "残ります。"),
    ZH_HANS("{0} 中的每个相机都保持原样，通过匹配图像求出新的点。新点会替换模型的 "
            "points3D.bin，images.bin 也随之更新，位姿不变；原来的文件保留为 "
            "points3D.bin_original 和 images.bin_original。"),
    ZH_HANT("{0} 中的每個相機都保持原樣，透過比對影像求出新的點。新點會取代模型的 "
            "points3D.bin，images.bin 也隨之更新，位姿不變；原來的檔案保留為 "
            "points3D.bin_original 和 images.bin_original。"),
    KO("{0}의 모든 카메라를 그대로 두고 이미지를 서로 매칭해 새 점을 찾습니다. 새 점이 "
       "모델의 points3D.bin을 대체하고, images.bin도 포즈는 그대로 둔 채 맞춰 갱신됩니다. "
       "이전 파일은 points3D.bin_original과 images.bin_original로 남습니다."),
    DE("Behält jede Kamera in {0} genau bei und findet neue Punkte, indem die Bilder "
       "abgeglichen werden. Sie ersetzen die points3D.bin des Modells, und images.bin wird "
       "passend aktualisiert, die Posen unverändert; die bisherigen Dateien bleiben als "
       "points3D.bin_original und images.bin_original erhalten."),
    FR("Garde chaque caméra de {0} telle quelle et trouve de nouveaux points en appariant "
       "les images. Ils remplacent le points3D.bin du modèle, et images.bin est mis à jour "
       "en conséquence, poses inchangées ; les fichiers précédents sont gardés sous les noms "
       "points3D.bin_original et images.bin_original."),
    ES("Mantiene cada cámara de {0} exactamente como está y encuentra puntos nuevos "
       "emparejando las imágenes. Sustituyen el points3D.bin del modelo, e images.bin se "
       "actualiza a juego con las poses intactas; los archivos anteriores se guardan como "
       "points3D.bin_original e images.bin_original."),
    PT("Mantém cada câmera de {0} exatamente como está e encontra novos pontos "
       "correspondendo as imagens. Eles substituem o points3D.bin do modelo, e o images.bin "
       "é atualizado de acordo, com as poses inalteradas; os arquivos anteriores ficam como "
       "points3D.bin_original e images.bin_original."),
    IT("Mantiene ogni fotocamera di {0} esattamente com'è e trova nuovi punti abbinando le "
       "immagini. Sostituiscono il points3D.bin del modello, e images.bin viene aggiornato "
       "di conseguenza con le pose invariate; i file precedenti restano come "
       "points3D.bin_original e images.bin_original."),
    NL("Houdt elke camera in {0} precies zoals hij is en vindt nieuwe punten door de "
       "beelden te matchen. Ze vervangen de points3D.bin van het model, en images.bin wordt "
       "erop afgestemd met ongewijzigde standen; de vorige bestanden blijven bewaard als "
       "points3D.bin_original en images.bin_original."),
    RU("Оставляет каждую камеру из {0} в точности как есть и находит новые точки, "
       "сопоставляя изображения. Они заменяют points3D.bin модели, а images.bin обновляется "
       "под них без изменения поз; прежние файлы сохраняются как points3D.bin_original и "
       "images.bin_original."),
    TR("{0} içindeki her kamerayı olduğu gibi tutar ve görüntüleri eşleştirerek yeni "
       "noktalar bulur. Bunlar modelin points3D.bin dosyasının yerini alır, images.bin de "
       "pozlar değişmeden buna göre güncellenir; önceki dosyalar points3D.bin_original ve "
       "images.bin_original olarak saklanır."));

SS_MSG(use_masks,
    EN("Leave what the masks cover out of matching"),
    JA("マスクで覆われた部分を照合から除外"), ZH_HANS("匹配时排除遮罩覆盖的部分"),
    ZH_HANT("比對時排除遮罩覆蓋的部分"), KO("마스크가 덮은 부분은 매칭에서 제외"),
    DE("Was die Masken abdecken, vom Abgleich ausnehmen"),
    FR("Exclure de l'appariement ce que couvrent les masques"),
    ES("Excluir del emparejamiento lo que cubren las máscaras"),
    PT("Excluir da correspondência o que as máscaras cobrem"),
    IT("Escludi dall'abbinamento ciò che coprono le maschere"),
    NL("Wat de maskers bedekken buiten het matchen laten"),
    RU("Не сопоставлять то, что закрыто масками"),
    TR("Maskelerin kapladığı yerleri eşleştirmenin dışında bırak"));

SS_MSG(run,
    EN("Recompute"), JA("再計算"), ZH_HANS("重新计算"), ZH_HANT("重新計算"), KO("재계산"),
    DE("Neu berechnen"), FR("Recalculer"), ES("Recalcular"), PT("Recalcular"),
    IT("Ricalcola"), NL("Opnieuw berekenen"), RU("Пересчитать"), TR("Yeniden hesapla"));

SS_MSG(stage_points,
    EN("Adding points to the imported cameras"), JA("取り込んだカメラに点を追加しています"),
    ZH_HANS("正在为导入的相机添加点"), ZH_HANT("正在為匯入的相機新增點"),
    KO("가져온 카메라에 점을 추가하는 중"), DE("Punkte zu den importierten Kameras hinzufügen"),
    FR("Ajout de points aux caméras importées"), ES("Añadiendo puntos a las cámaras importadas"),
    PT("Adicionando pontos às câmeras importadas"),
    IT("Aggiunta di punti alle fotocamere importate"),
    NL("Punten toevoegen aan de geïmporteerde camera's"),
    RU("Добавление точек к импортированным камерам"),
    TR("İçe aktarılan kameralara noktalar ekleniyor"));

SS_MSG(stage_writing,
    EN("Writing the model"), JA("モデルを書き出しています"), ZH_HANS("正在写出模型"),
    ZH_HANT("正在寫出模型"), KO("모델을 기록하는 중"), DE("Modell wird geschrieben"),
    FR("Écriture du modèle"), ES("Escribiendo el modelo"), PT("Gravando o modelo"),
    IT("Scrittura del modello"), NL("Het model wegschrijven"), RU("Запись модели"),
    TR("Model yazılıyor"));

// {0} points, {1} cameras, {2} the model folder.
SS_MSG(done,
    EN("Recomputed the sparse point cloud: {0} points for {1} cameras, now in {2}. The "
       "previous files are kept as points3D.bin_original and images.bin_original."),
    JA("スパース点群を再計算しました: カメラ {1} 台に対して {0} 点、{2} に入りました。"
       "以前のファイルは points3D.bin_original と images.bin_original として残ります。"),
    ZH_HANS("已重新计算稀疏点云：{1} 个相机共 {0} 个点，现位于 {2}。原来的文件保留为 "
            "points3D.bin_original 和 images.bin_original。"),
    ZH_HANT("已重新計算稀疏點雲：{1} 個相機共 {0} 個點，現位於 {2}。原來的檔案保留為 "
            "points3D.bin_original 和 images.bin_original。"),
    KO("희소 포인트 클라우드를 다시 계산했습니다: 카메라 {1}대에 점 {0}개, 이제 {2}에 "
       "있습니다. 이전 파일은 points3D.bin_original과 images.bin_original로 남습니다."),
    DE("Dünne Punktwolke neu berechnet: {0} Punkte für {1} Kameras, jetzt in {2}. Die "
       "bisherigen Dateien bleiben als points3D.bin_original und images.bin_original."),
    FR("Nuage de points épars recalculé : {0} points pour {1} caméras, désormais dans {2}. "
       "Les fichiers précédents sont gardés sous points3D.bin_original et "
       "images.bin_original."),
    ES("Nube de puntos dispersa recalculada: {0} puntos para {1} cámaras, ahora en {2}. "
       "Los archivos anteriores se guardan como points3D.bin_original e images.bin_original."),
    PT("Nuvem de pontos esparsa recalculada: {0} pontos para {1} câmeras, agora em {2}. Os "
       "arquivos anteriores ficam como points3D.bin_original e images.bin_original."),
    IT("Nuvola di punti sparsa ricalcolata: {0} punti per {1} fotocamere, ora in {2}. I "
       "file precedenti restano come points3D.bin_original e images.bin_original."),
    NL("Dunne puntenwolk opnieuw berekend: {0} punten voor {1} camera's, nu in {2}. De "
       "vorige bestanden blijven bewaard als points3D.bin_original en images.bin_original."),
    RU("Разреженное облако пересчитано: {0} точек для {1} камер, теперь в {2}. Прежние "
       "файлы сохранены как points3D.bin_original и images.bin_original."),
    TR("Seyrek nokta bulutu yeniden hesaplandı: {1} kamera için {0} nokta, artık {2} "
       "içinde. Önceki dosyalar points3D.bin_original ve images.bin_original olarak "
       "saklanıyor."));

SS_MSG(failed,
    EN("Recomputing the sparse point cloud failed (exit code {0}); the lines above say why."),
    JA("スパース点群の再計算に失敗しました（終了コード {0}）。理由は上の行にあります。"),
    ZH_HANS("重新计算稀疏点云失败（退出码 {0}），原因见上面几行。"),
    ZH_HANT("重新計算稀疏點雲失敗（結束碼 {0}），原因見上面幾行。"),
    KO("희소 포인트 클라우드 재계산에 실패했습니다(종료 코드 {0}). 이유는 위 줄에 있습니다."),
    DE("Neuberechnung der dünnen Punktwolke fehlgeschlagen (Exit-Code {0}); der Grund "
       "steht in den Zeilen darüber."),
    FR("Le recalcul du nuage de points épars a échoué (code de sortie {0}) ; les lignes "
       "ci-dessus disent pourquoi."),
    ES("El recálculo de la nube de puntos dispersa falló (código de salida {0}); las "
       "líneas de arriba dicen por qué."),
    PT("O recálculo da nuvem de pontos esparsa falhou (código de saída {0}); as linhas "
       "acima dizem por quê."),
    IT("Il ricalcolo della nuvola di punti sparsa non è riuscito (codice di uscita {0}); "
       "le righe sopra dicono perché."),
    NL("Opnieuw berekenen van de dunne puntenwolk mislukt (afsluitcode {0}); de regels "
       "hierboven zeggen waarom."),
    RU("Не удалось пересчитать разреженное облако (код выхода {0}); причина в строках выше."),
    TR("Seyrek nokta bulutu yeniden hesaplanamadı (çıkış kodu {0}); nedeni yukarıdaki "
       "satırlarda."));

SS_MSG(cancelled,
    EN("Recomputing the sparse point cloud was stopped."),
    JA("スパース点群の再計算を中止しました。"), ZH_HANS("已停止重新计算稀疏点云。"),
    ZH_HANT("已停止重新計算稀疏點雲。"), KO("희소 포인트 클라우드 재계산을 중지했습니다."),
    DE("Neuberechnung der dünnen Punktwolke abgebrochen."),
    FR("Le recalcul du nuage de points épars a été arrêté."),
    ES("Se detuvo el recálculo de la nube de puntos dispersa."),
    PT("O recálculo da nuvem de pontos esparsa foi interrompido."),
    IT("Il ricalcolo della nuvola di punti sparsa è stato interrotto."),
    NL("Opnieuw berekenen van de dunne puntenwolk is gestopt."),
    RU("Пересчёт разреженного облака остановлен."),
    TR("Seyrek nokta bulutunun yeniden hesaplanması durduruldu."));

// {0} the program that could not be started.
SS_MSG(spawn_failed,
    EN("Could not start {0}."), JA("{0} を起動できませんでした。"), ZH_HANS("无法启动 {0}。"),
    ZH_HANT("無法啟動 {0}。"), KO("{0}을(를) 시작할 수 없습니다."),
    DE("{0} konnte nicht gestartet werden."), FR("Impossible de lancer {0}."),
    ES("No se pudo iniciar {0}."), PT("Não foi possível iniciar {0}."),
    IT("Impossibile avviare {0}."), NL("{0} kon niet worden gestart."),
    RU("Не удалось запустить {0}."), TR("{0} başlatılamadı."));

SS_MSG(restore,
    EN("Restore the Original Points"), JA("元の点に戻す"), ZH_HANS("恢复原来的点"),
    ZH_HANT("還原原來的點"), KO("원래 점으로 되돌리기"), DE("Ursprüngliche Punkte wiederherstellen"),
    FR("Rétablir les points d'origine"), ES("Restaurar los puntos originales"),
    PT("Restaurar os pontos originais"), IT("Ripristina i punti originali"),
    NL("Oorspronkelijke punten terugzetten"), RU("Вернуть исходные точки"),
    TR("Özgün noktaları geri yükle"));

SS_MSG(restore_help,
    EN("Puts points3D.bin_original and images.bin_original back in place of the "
       "recomputed files."),
    JA("再計算したファイルの代わりに points3D.bin_original と images.bin_original を元に"
       "戻します。"),
    ZH_HANS("用 points3D.bin_original 和 images.bin_original 换回重新计算的文件。"),
    ZH_HANT("用 points3D.bin_original 和 images.bin_original 換回重新計算的檔案。"),
    KO("재계산한 파일 대신 points3D.bin_original과 images.bin_original을 되돌려 놓습니다."),
    DE("Setzt points3D.bin_original und images.bin_original wieder an die Stelle der neu "
       "berechneten Dateien."),
    FR("Remet points3D.bin_original et images.bin_original à la place des fichiers "
       "recalculés."),
    ES("Vuelve a poner points3D.bin_original e images.bin_original en lugar de los "
       "archivos recalculados."),
    PT("Coloca points3D.bin_original e images.bin_original de volta no lugar dos arquivos "
       "recalculados."),
    IT("Rimette points3D.bin_original e images.bin_original al posto dei file ricalcolati."),
    NL("Zet points3D.bin_original en images.bin_original terug in plaats van de opnieuw "
       "berekende bestanden."),
    RU("Возвращает points3D.bin_original и images.bin_original на место пересчитанных "
       "файлов."),
    TR("Yeniden hesaplanan dosyaların yerine points3D.bin_original ve images.bin_original "
       "dosyalarını geri koyar."));

// {0} the model folder.
SS_MSG(restored,
    EN("Restored the original points in {0}."), JA("{0} の元の点に戻しました。"),
    ZH_HANS("已恢复 {0} 中原来的点。"), ZH_HANT("已還原 {0} 中原來的點。"),
    KO("{0}의 원래 점으로 되돌렸습니다."), DE("Ursprüngliche Punkte in {0} wiederhergestellt."),
    FR("Points d'origine rétablis dans {0}."), ES("Se restauraron los puntos originales en {0}."),
    PT("Pontos originais restaurados em {0}."), IT("Punti originali ripristinati in {0}."),
    NL("Oorspronkelijke punten in {0} teruggezet."), RU("Исходные точки в {0} восстановлены."),
    TR("{0} içindeki özgün noktalar geri yüklendi."));

// {0} the model folder, {1} the system's reason.
SS_MSG(install_failed,
    EN("Could not change the files in {0}: {1}"), JA("{0} のファイルを変更できませんでした: {1}"),
    ZH_HANS("无法修改 {0} 中的文件：{1}"), ZH_HANT("無法修改 {0} 中的檔案：{1}"),
    KO("{0}의 파일을 바꿀 수 없습니다: {1}"), DE("Dateien in {0} konnten nicht geändert werden: {1}"),
    FR("Impossible de modifier les fichiers de {0} : {1}"),
    ES("No se pudieron cambiar los archivos de {0}: {1}"),
    PT("Não foi possível alterar os arquivos em {0}: {1}"),
    IT("Impossibile modificare i file in {0}: {1}"),
    NL("Kon de bestanden in {0} niet wijzigen: {1}"),
    RU("Не удалось изменить файлы в {0}: {1}"), TR("{0} içindeki dosyalar değiştirilemedi: {1}"));

}  // namespace recompute
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
