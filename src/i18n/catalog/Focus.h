#pragma once

// What `spirula focus` says: its --help and the lines a run prints.

#include "i18n/BeginCatalog.h"

namespace spirula {
namespace i18n {
namespace msg {
namespace focus {

SS_MSG(tagline,
    EN("Per-pixel focus weights for a dataset: how sharp each pixel is"),
    JA("データセットの画素ごとの合焦重み：各画素がどれだけ鮮明か"),
    ZH_HANS("数据集的逐像素实焦权重：每个像素有多清晰"),
    ZH_HANT("資料集的逐像素實焦權重：每個像素有多清晰"),
    KO("데이터셋의 픽셀별 초점 가중치: 각 픽셀이 얼마나 선명한지"),
    DE("Fokusgewichte pro Pixel für einen Datensatz: wie scharf jedes Pixel ist"),
    FR("Poids de mise au point par pixel : la netteté de chaque pixel"),
    ES("Pesos de enfoque por píxel de un conjunto: lo nítido que es cada píxel"),
    PT("Pesos de foco por pixel de um conjunto: o quão nítido é cada pixel"),
    IT("Pesi di messa a fuoco per pixel: quanto è nitido ogni pixel"),
    NL("Focusgewichten per pixel voor een dataset: hoe scherp elke pixel is"),
    RU("Веса резкости по пикселям для набора: насколько резок каждый пиксель"),
    TR("Bir veri kümesi için piksel başına odak ağırlıkları: her piksel ne kadar net"));

SS_MSG(usage_target,
    EN("The dataset folder. Reads depths/ (from `geometry --depth`), measures "
       "edge blur in each photo, fits how blur grows away from the focal plane "
       "and writes 8-bit weights to focus/: white counts fully in training, "
       "black not at all. Training multiplies them into masks/."),
    JA("データセットのフォルダです。depths/（`geometry --depth` の出力）を読み、"
       "各写真のエッジのぼけを測り、焦点面から離れるにつれてぼけが増える様子を"
       "当てはめ、8 ビットの重みを focus/ に書き出します。白は学習で完全に使われ、"
       "黒は使われません。学習時に masks/ と掛け合わされます。"),
    ZH_HANS("数据集文件夹。读取 depths/（来自 `geometry --depth`），测量每张照片的"
            "边缘模糊，拟合模糊随离开焦平面而增长的规律，并把 8 位权重写入 focus/："
            "白色在训练中完全计入，黑色完全不计入。训练时会与 masks/ 相乘。"),
    ZH_HANT("資料集資料夾。讀取 depths/（來自 `geometry --depth`），量測每張照片的"
            "邊緣模糊，擬合模糊隨離開焦平面而增長的規律，並把 8 位元權重寫入 focus/："
            "白色在訓練中完全計入，黑色完全不計入。訓練時會與 masks/ 相乘。"),
    KO("데이터셋 폴더입니다. depths/(`geometry --depth` 출력)를 읽고, 각 사진의 "
       "가장자리 흐림을 측정해 초점면에서 멀어질수록 흐림이 커지는 정도를 맞춘 뒤 "
       "8비트 가중치를 focus/ 에 씁니다. 흰색은 학습에 완전히 반영되고 검은색은 "
       "반영되지 않습니다. 학습 시 masks/ 와 곱해집니다."),
    DE("Der Datensatzordner. Liest depths/ (aus `geometry --depth`), misst die "
       "Kantenunschärfe jedes Fotos, passt an, wie die Unschärfe abseits der "
       "Fokusebene wächst, und schreibt 8-Bit-Gewichte nach focus/: Weiß zählt "
       "im Training voll, Schwarz gar nicht. Das Training multipliziert sie in "
       "masks/ hinein."),
    FR("Le dossier du jeu de données. Lit depths/ (issu de `geometry --depth`), "
       "mesure le flou des contours de chaque photo, ajuste la croissance du flou "
       "loin du plan de netteté et écrit des poids 8 bits dans focus/ : le blanc "
       "compte pleinement à l'entraînement, le noir pas du tout. L'entraînement "
       "les multiplie aux masks/."),
    ES("La carpeta del conjunto. Lee depths/ (de `geometry --depth`), mide el "
       "desenfoque de los bordes de cada foto, ajusta cómo crece al alejarse del "
       "plano de enfoque y escribe pesos de 8 bits en focus/: el blanco cuenta "
       "por completo en el entrenamiento y el negro nada. El entrenamiento los "
       "multiplica por masks/."),
    PT("A pasta do conjunto. Lê depths/ (de `geometry --depth`), mede o desfoque "
       "das arestas de cada foto, ajusta como ele cresce longe do plano de foco e "
       "escreve pesos de 8 bits em focus/: o branco conta por inteiro no treino e "
       "o preto nada. O treino multiplica-os pelas masks/."),
    IT("La cartella dell'insieme. Legge depths/ (da `geometry --depth`), misura "
       "la sfocatura dei bordi di ogni foto, adatta come cresce lontano dal piano "
       "di fuoco e scrive pesi a 8 bit in focus/: il bianco conta del tutto "
       "nell'addestramento, il nero per niente. L'addestramento li moltiplica "
       "per masks/."),
    NL("De datasetmap. Leest depths/ (uit `geometry --depth`), meet de randonscherpte "
       "in elke foto, past aan hoe die groeit buiten het scherpstelvlak en schrijft "
       "8-bits gewichten naar focus/: wit telt volledig mee in de training, zwart "
       "helemaal niet. De training vermenigvuldigt ze met masks/."),
    RU("Папка набора. Читает depths/ (из `geometry --depth`), измеряет размытие "
       "краёв на каждом снимке, подбирает, как оно растёт вдали от плоскости "
       "фокусировки, и пишет 8-битные веса в focus/: белое учитывается в обучении "
       "полностью, чёрное — никак. При обучении они умножаются на masks/."),
    TR("Veri kümesi klasörü. depths/ (`geometry --depth` çıktısı) okunur, her "
       "fotoğraftaki kenar bulanıklığı ölçülür, odak düzleminden uzaklaştıkça "
       "nasıl arttığı uydurulur ve 8 bit ağırlıklar focus/ içine yazılır: beyaz "
       "eğitimde tam sayılır, siyah hiç sayılmaz. Eğitim bunları masks/ ile çarpar."));

SS_MSG(opt_depth_dir,
    EN("Depth maps, relative to the dataset (default depths)"),
    JA("深度マップ。データセットからの相対パス（既定 depths）"),
    ZH_HANS("深度图，相对于数据集（默认 depths）"),
    ZH_HANT("深度圖，相對於資料集（預設 depths）"),
    KO("깊이 맵, 데이터셋 기준 상대 경로 (기본값 depths)"),
    DE("Tiefenkarten, relativ zum Datensatz (Standard depths)"),
    FR("Cartes de profondeur, relatives au jeu de données (défaut depths)"),
    ES("Mapas de profundidad, relativos al conjunto (por defecto depths)"),
    PT("Mapas de profundidade, relativos ao conjunto (padrão depths)"),
    IT("Mappe di profondità, relative all'insieme (predefinito depths)"),
    NL("Dieptekaarten, relatief aan de dataset (standaard depths)"),
    RU("Карты глубины относительно набора (по умолчанию depths)"),
    TR("Derinlik haritaları, veri kümesine göre (varsayılan depths)"));

SS_MSG(opt_out_dir,
    EN("Where the weights go, relative to the dataset (default focus)"),
    JA("重みの書き出し先。データセットからの相対パス（既定 focus）"),
    ZH_HANS("权重的输出位置，相对于数据集（默认 focus）"),
    ZH_HANT("權重的輸出位置，相對於資料集（預設 focus）"),
    KO("가중치를 쓸 위치, 데이터셋 기준 상대 경로 (기본값 focus)"),
    DE("Ziel der Gewichte, relativ zum Datensatz (Standard focus)"),
    FR("Destination des poids, relative au jeu de données (défaut focus)"),
    ES("Destino de los pesos, relativo al conjunto (por defecto focus)"),
    PT("Destino dos pesos, relativo ao conjunto (padrão focus)"),
    IT("Destinazione dei pesi, relativa all'insieme (predefinito focus)"),
    NL("Doel van de gewichten, relatief aan de dataset (standaard focus)"),
    RU("Куда писать веса, относительно набора (по умолчанию focus)"),
    TR("Ağırlıkların yazılacağı yer, veri kümesine göre (varsayılan focus)"));

SS_MSG(opt_allowed,
    EN("Defocus blur allowed before a pixel loses weight, in pixels at the "
       "training resolution (default 1.4; at the limit the weight is half)"),
    JA("画素の重みが下がり始めるまでに許すぼけ。学習解像度での画素数"
       "（既定 1.4。上限ちょうどで重みは半分）"),
    ZH_HANS("像素开始降权前允许的离焦模糊，单位为训练分辨率下的像素"
            "（默认 1.4；恰好等于该值时权重为一半）"),
    ZH_HANT("像素開始降權前允許的離焦模糊，單位為訓練解析度下的像素"
            "（預設 1.4；恰好等於該值時權重為一半）"),
    KO("픽셀 가중치가 줄기 전까지 허용하는 초점 흐림, 학습 해상도 기준 픽셀 수 "
       "(기본값 1.4, 한계값에서 가중치는 절반)"),
    DE("Erlaubte Defokusunschärfe, bevor ein Pixel Gewicht verliert, in Pixeln "
       "der Trainingsauflösung (Standard 1.4; an der Grenze halbes Gewicht)"),
    FR("Flou de défocalisation toléré avant qu'un pixel perde du poids, en pixels "
       "à la résolution d'entraînement (défaut 1.4 ; à la limite, poids moitié)"),
    ES("Desenfoque permitido antes de que un píxel pierda peso, en píxeles a la "
       "resolución de entrenamiento (por defecto 1.4; en el límite, la mitad)"),
    PT("Desfoque permitido antes de um pixel perder peso, em pixels na resolução "
       "de treino (padrão 1.4; no limite, o peso é metade)"),
    IT("Sfocatura consentita prima che un pixel perda peso, in pixel alla "
       "risoluzione di addestramento (predefinito 1.4; al limite, peso metà)"),
    NL("Toegestane onscherpte voordat een pixel gewicht verliest, in pixels op "
       "de trainingsresolutie (standaard 1.4; op de grens half gewicht)"),
    RU("Допустимое размытие расфокуса до потери веса, в пикселях при разрешении "
       "обучения (по умолчанию 1.4; на границе вес вдвое меньше)"),
    TR("Bir pikselin ağırlık kaybetmeden önce izin verilen odak bulanıklığı, "
       "eğitim çözünürlüğünde piksel (varsayılan 1.4; sınırda ağırlık yarıdır)"));

SS_MSG(opt_train_side,
    EN("Longest side the dataset is trained at, in pixels (default 5760; "
       "capped at the photo's own)"),
    JA("学習時の長辺の画素数（既定 5760。写真自体の長辺が上限）"),
    ZH_HANS("训练时的长边像素数（默认 5760；不超过照片本身的长边）"),
    ZH_HANT("訓練時的長邊像素數（預設 5760；不超過照片本身的長邊）"),
    KO("학습 시 긴 변의 픽셀 수 (기본값 5760, 사진 자체 크기가 상한)"),
    DE("Längste Seite beim Training in Pixeln (Standard 5760; höchstens die des Fotos)"),
    FR("Plus grand côté à l'entraînement, en pixels (défaut 5760 ; au plus celui de la photo)"),
    ES("Lado mayor al entrenar, en píxeles (por defecto 5760; como máximo el de la foto)"),
    PT("Lado maior no treino, em pixels (padrão 5760; no máximo o da foto)"),
    IT("Lato maggiore in addestramento, in pixel (predefinito 5760; al massimo quello della foto)"),
    NL("Langste zijde bij training in pixels (standaard 5760; hoogstens die van de foto)"),
    RU("Длинная сторона при обучении в пикселях (по умолчанию 5760; не больше снимка)"),
    TR("Eğitimdeki uzun kenar, piksel (varsayılan 5760; en fazla fotoğrafınki)"));

SS_MSG(opt_overwrite,
    EN("Recompute weights that already exist"),
    JA("既存の重みも計算し直す"),
    ZH_HANS("重新计算已存在的权重"),
    ZH_HANT("重新計算已存在的權重"),
    KO("이미 있는 가중치도 다시 계산"),
    DE("Vorhandene Gewichte neu berechnen"),
    FR("Recalculer les poids déjà présents"),
    ES("Volver a calcular los pesos existentes"),
    PT("Recalcular os pesos já existentes"),
    IT("Ricalcolare i pesi già presenti"),
    NL("Bestaande gewichten opnieuw berekenen"),
    RU("Пересчитать уже существующие веса"),
    TR("Var olan ağırlıkları yeniden hesapla"));

SS_MSG(log_image,
    EN("{0}: mean weight {1}, fit error {2} px"),
    JA("{0}: 平均重み {1}、当てはめ誤差 {2} px"),
    ZH_HANS("{0}: 平均权重 {1}，拟合误差 {2} px"),
    ZH_HANT("{0}: 平均權重 {1}，擬合誤差 {2} px"),
    KO("{0}: 평균 가중치 {1}, 맞춤 오차 {2} px"),
    DE("{0}: mittleres Gewicht {1}, Anpassungsfehler {2} px"),
    FR("{0} : poids moyen {1}, erreur d'ajustement {2} px"),
    ES("{0}: peso medio {1}, error de ajuste {2} px"),
    PT("{0}: peso médio {1}, erro de ajuste {2} px"),
    IT("{0}: peso medio {1}, errore di adattamento {2} px"),
    NL("{0}: gemiddeld gewicht {1}, passingsfout {2} px"),
    RU("{0}: средний вес {1}, ошибка подгонки {2} px"),
    TR("{0}: ortalama ağırlık {1}, uydurma hatası {2} px"));

SS_MSG(warn_no_fit,
    EN("{0}: too few sharp edges to fit; weights left at 1"),
    JA("{0}: 当てはめに使える鮮明なエッジが少なすぎます。重みは 1 のままです"),
    ZH_HANS("{0}: 可用于拟合的清晰边缘太少，权重保持为 1"),
    ZH_HANT("{0}: 可用於擬合的清晰邊緣太少，權重保持為 1"),
    KO("{0}: 맞춤에 쓸 선명한 가장자리가 너무 적어 가중치를 1로 둡니다"),
    DE("{0}: zu wenige scharfe Kanten für die Anpassung; Gewichte bleiben 1"),
    FR("{0} : trop peu de contours nets pour l'ajustement ; poids laissés à 1"),
    ES("{0}: muy pocos bordes nítidos para ajustar; los pesos quedan en 1"),
    PT("{0}: poucas arestas nítidas para ajustar; os pesos ficam em 1"),
    IT("{0}: troppo pochi bordi nitidi per l'adattamento; pesi lasciati a 1"),
    NL("{0}: te weinig scherpe randen om te passen; gewichten blijven 1"),
    RU("{0}: слишком мало резких краёв для подгонки; веса оставлены равными 1"),
    TR("{0}: uydurma için çok az net kenar; ağırlıklar 1 bırakıldı"));

SS_MSG(err_no_depth,
    EN("{0} has no depth map in {1}; run `{2} geometry --depth` first"),
    JA("{0} の深度マップが {1} にありません。先に `{2} geometry --depth` を実行してください"),
    ZH_HANS("{1} 中没有 {0} 的深度图；请先运行 `{2} geometry --depth`"),
    ZH_HANT("{1} 中沒有 {0} 的深度圖；請先執行 `{2} geometry --depth`"),
    KO("{1} 에 {0} 의 깊이 맵이 없습니다. 먼저 `{2} geometry --depth` 를 실행하세요"),
    DE("{0} hat keine Tiefenkarte in {1}; zuerst `{2} geometry --depth` ausführen"),
    FR("{0} n'a pas de carte de profondeur dans {1} ; lancez d'abord `{2} geometry --depth`"),
    ES("{0} no tiene mapa de profundidad en {1}; ejecuta antes `{2} geometry --depth`"),
    PT("{0} não tem mapa de profundidade em {1}; execute antes `{2} geometry --depth`"),
    IT("{0} non ha una mappa di profondità in {1}; esegui prima `{2} geometry --depth`"),
    NL("{0} heeft geen dieptekaart in {1}; voer eerst `{2} geometry --depth` uit"),
    RU("Для {0} нет карты глубины в {1}; сначала запустите `{2} geometry --depth`"),
    TR("{0} için {1} içinde derinlik haritası yok; önce `{2} geometry --depth` çalıştırın"));

SS_MSG(log_done,
    EN("{0} weight maps written, {1} skipped, {2} without depth"),
    JA("重みマップ {0} 枚を書き出し、{1} 枚をスキップ、深度なし {2} 枚"),
    ZH_HANS("已写入 {0} 张权重图，跳过 {1} 张，缺少深度 {2} 张"),
    ZH_HANT("已寫入 {0} 張權重圖，略過 {1} 張，缺少深度 {2} 張"),
    KO("가중치 맵 {0} 장 기록, {1} 장 건너뜀, 깊이 없음 {2} 장"),
    DE("{0} Gewichtskarten geschrieben, {1} übersprungen, {2} ohne Tiefe"),
    FR("{0} cartes de poids écrites, {1} ignorées, {2} sans profondeur"),
    ES("{0} mapas de pesos escritos, {1} omitidos, {2} sin profundidad"),
    PT("{0} mapas de pesos escritos, {1} ignorados, {2} sem profundidade"),
    IT("{0} mappe di pesi scritte, {1} saltate, {2} senza profondità"),
    NL("{0} gewichtskaarten geschreven, {1} overgeslagen, {2} zonder diepte"),
    RU("Записано карт весов: {0}, пропущено: {1}, без глубины: {2}"),
    TR("{0} ağırlık haritası yazıldı, {1} atlandı, {2} derinliksiz"));

}  // namespace focus
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
