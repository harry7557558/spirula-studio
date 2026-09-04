#pragma once

// Training vocabulary shared by the GUI and `spirula train --help`.
//
// The presets are named on the command line ("spirula train 360-camera"), so
// their NAMES are identifiers and live in config/TrainConfig.h with the
// appliers. Only what a human reads is here: a short label for the picker and
// the sentence that explains when to reach for it. Both consumers read this
// table, so there is one copy of the text and `--help` is localized too.
//
// config/TrainConfig.h deliberately does not include this file: it is a plain
// config header, included nearly everywhere, and it should not drag a
// translation catalog behind it. The link between the two lists is a
// static_assert at each consumer that the counts match.

#include "i18n/BeginCatalog.h"

#include <cstddef>
#include <cstring>

namespace spirula {
namespace i18n {
namespace msg {
namespace train {

// ===========================================================================
// Presets
// ===========================================================================

SS_MSG(preset_3dgs,
    EN("General purpose"),
    JA("汎用"),
    ZH_HANS("通用"),
    ZH_HANT("通用"),
    KO("범용"),
    DE("Allzweck"),
    FR("Usage général"),
    ES("Uso general"),
    PT("Uso geral"),
    IT("Uso generale"),
    NL("Algemeen"),
    RU("Универсальный"),
    TR("Genel amaçlı"));

SS_MSG(preset_3dgs_help,
    EN("Generic method that works well for most datasets."),
    JA("ほとんどのデータセットでうまく動く一般的な方法です。"),
    ZH_HANS("通用方法，适用于大多数数据集。"),
    ZH_HANT("通用方法，適用於大多數資料集。"),
    KO("대부분의 데이터셋에서 잘 동작하는 일반적인 방법입니다."),
    DE("Allgemeines Verfahren, das mit den meisten Datensätzen gut "
       "funktioniert."),
    FR("Méthode générique qui fonctionne bien sur la plupart des jeux de "
       "données."),
    ES("Método genérico que funciona bien con la mayoría de los conjuntos de "
       "datos."),
    PT("Método genérico que funciona bem na maioria dos conjuntos de dados."),
    IT("Metodo generico che funziona bene con la maggior parte dei set di "
       "dati."),
    NL("Algemene methode die voor de meeste datasets goed werkt."),
    RU("Универсальный метод, хорошо работающий с большинством наборов данных."),
    TR("Çoğu veri kümesinde iyi çalışan genel yöntem."));

SS_MSG(preset_360_camera,
    EN("360 camera"),
    JA("360度カメラ"),
    ZH_HANS("360 度相机"),
    ZH_HANT("360 度相機"),
    KO("360도 카메라"),
    DE("360-Grad-Kamera"),
    FR("Caméra 360"),
    ES("Cámara 360"),
    PT("Câmera 360"),
    IT("Fotocamera 360"),
    NL("360-gradencamera"),
    RU("Камера 360"),
    TR("360 derece kamera"));

SS_MSG(preset_360_camera_help,
    EN("Preset for training on original distorted images captured by 360 "
       "cameras. Recommended if your dataset contains very wide fisheye "
       "images with a circle visible."),
    JA("360度カメラで撮った歪んだ元画像のまま学習するためのプリセットです。"
       "画角が非常に広く、円形に写った魚眼画像が含まれるデータセットに"
       "向いています。"),
    ZH_HANS("用于直接在 360 相机拍摄的原始畸变图像上训练。如果数据集里的鱼眼"
            "图像视角很大、能看到圆形边界，推荐用它。"),
    ZH_HANT("用於直接在 360 相機拍攝的原始變形影像上訓練。若資料集裡的魚眼"
            "影像視角很大、看得到圓形邊界，建議使用。"),
    KO("360도 카메라로 찍은 왜곡된 원본 이미지를 그대로 학습하기 위한 "
       "프리셋입니다. 화각이 아주 넓고 원형이 보이는 어안 이미지가 들어 있는 "
       "데이터셋에 알맞습니다."),
    DE("Voreinstellung für das Training auf den unbearbeiteten, verzeichneten "
       "Bildern von 360-Grad-Kameras. Empfohlen, wenn der Datensatz sehr "
       "weitwinklige Fisheye-Bilder mit sichtbarem Kreis enthält."),
    FR("Préréglage pour l'entraînement sur les images d'origine, distordues, "
       "des caméras 360. Recommandé si le jeu de données contient des images "
       "fisheye très ouvertes où le cercle est visible."),
    ES("Preajuste para entrenar con las imágenes originales distorsionadas de "
       "cámaras 360. Recomendado si el conjunto de datos contiene imágenes de "
       "ojo de pez muy angulares con el círculo visible."),
    PT("Predefinição para treinar com as imagens originais distorcidas de "
       "câmeras 360. Recomendada se o conjunto de dados contiver imagens "
       "olho-de-peixe muito abertas com o círculo visível."),
    IT("Preimpostazione per addestrare sulle immagini originali distorte delle "
       "fotocamere 360. Consigliata se il set di dati contiene immagini "
       "fisheye molto aperte con il cerchio visibile."),
    NL("Voorinstelling om te trainen op de originele, vervormde beelden van "
       "360-gradencamera's. Aanbevolen als de dataset zeer wijde "
       "fisheyebeelden met een zichtbare cirkel bevat."),
    RU("Пресет для обучения на исходных искажённых снимках камер 360. "
       "Рекомендуется, если в наборе данных есть очень широкие кадры «рыбий "
       "глаз» с видимым кругом."),
    TR("360 derece kameraların çektiği özgün, bozuk görüntüler üzerinde "
       "eğitim için hazır ayar. Veri kümenizde çemberi görünen, çok geniş "
       "açılı balıkgözü görüntüler varsa önerilir."));

SS_MSG(preset_in_the_wild,
    EN("Internet or mixed photos"),
    JA("ネット上・寄せ集めの写真"),
    ZH_HANS("网络或混杂来源的照片"),
    ZH_HANT("網路或混雜來源的照片"),
    KO("인터넷·여러 출처의 사진"),
    DE("Internet- oder gemischte Fotos"),
    FR("Photos d'Internet ou d'origines diverses"),
    ES("Fotos de internet o de origen variado"),
    PT("Fotos da internet ou de origens variadas"),
    IT("Foto da internet o di origine mista"),
    NL("Internetfoto's of gemengde bronnen"),
    RU("Фотографии из интернета или разных источников"),
    TR("İnternetten veya karışık kaynaklı fotoğraflar"));

SS_MSG(preset_in_the_wild_help,
    EN("Preset for datasets consisting of internet images, with extreme "
       "lighting variation, with un-masked outliers, and/or shot with long "
       "focal lengths."),
    JA("ネット上の画像を集めたデータセット、明るさの差が極端なもの、余計な"
       "写り込みをマスクしていないもの、望遠で撮ったものに向いたプリセット"
       "です。"),
    ZH_HANS("适用于由网络图片组成的数据集，以及光照差异极大、未遮罩杂物、"
            "或用长焦拍摄的数据集。"),
    ZH_HANT("適用於由網路圖片組成的資料集，以及光線差異極大、未遮罩雜物、"
            "或用長焦拍攝的資料集。"),
    KO("인터넷 이미지를 모은 데이터셋, 밝기 차이가 극심한 데이터셋, 가리지 "
       "않은 이물이 있는 데이터셋, 망원으로 찍은 데이터셋에 맞는 프리셋입니다."),
    DE("Voreinstellung für Datensätze aus Internetbildern, mit extremen "
       "Helligkeitsunterschieden, mit nicht maskierten Störobjekten und/oder "
       "mit langen Brennweiten aufgenommen."),
    FR("Préréglage pour les jeux de données faits d'images trouvées en ligne, "
       "aux écarts d'éclairage extrêmes, aux intrus non masqués et/ou pris "
       "avec de longues focales."),
    ES("Preajuste para conjuntos de datos formados por imágenes de internet, "
       "con variaciones extremas de iluminación, con intrusos sin enmascarar "
       "y/o tomadas con focales largas."),
    PT("Predefinição para conjuntos de dados formados por imagens da internet, "
       "com variação extrema de iluminação, com intrusos não mascarados e/ou "
       "fotografados com distâncias focais longas."),
    IT("Preimpostazione per set di dati fatti di immagini trovate in rete, con "
       "variazioni di luce estreme, con intrusi non mascherati e/o scattate "
       "con focali lunghe."),
    NL("Voorinstelling voor datasets van internetbeelden, met extreme "
       "lichtverschillen, met niet-gemaskeerde stoorobjecten en/of met lange "
       "brandpuntsafstanden gemaakt."),
    RU("Пресет для наборов из интернет-снимков, с крайне разным освещением, с "
       "незамаскированными посторонними объектами и (или) снятых длиннофокусной "
       "оптикой."),
    TR("İnternetten toplanmış görüntülerden oluşan, aydınlatması aşırı "
       "değişken, maskelenmemiş yabancı nesneler içeren ve/veya uzun odaklı "
       "çekilmiş veri kümeleri için hazır ayar."));

SS_MSG(preset_centered_object,
    EN("Centered object"),
    JA("中心の被写体"),
    ZH_HANS("居中物体"),
    ZH_HANT("置中物體"),
    KO("가운데 놓인 물체"),
    DE("Zentriertes Objekt"),
    FR("Objet centré"),
    ES("Objeto centrado"),
    PT("Objeto centrado"),
    IT("Oggetto centrato"),
    NL("Gecentreerd object"),
    RU("Объект в центре"),
    TR("Merkezdeki nesne"));

SS_MSG(preset_centered_object_help,
    EN("Preset for captures that orbit a single masked object, whose "
       "background is not meant to be reconstructed."),
    JA("マスクした単一の被写体を取り囲むように撮影したデータのための"
       "プリセットです。背景は復元の対象にしません。"),
    ZH_HANS("用于环绕单个已遮罩物体拍摄的数据的预设，"
            "背景不作为重建对象。"),
    ZH_HANT("用於環繞單個已遮罩物體拍攝的資料的預設，"
            "背景不作為重建對象。"),
    KO("마스크한 하나의 물체를 둘러싸며 촬영한 데이터를 위한 "
       "프리셋입니다. 배경은 복원 대상이 아닙니다."),
    DE("Voreinstellung für Aufnahmen, die ein einzelnes maskiertes Objekt "
       "umkreisen und deren Hintergrund nicht rekonstruiert werden soll."),
    FR("Préréglage pour des prises de vue tournant autour d'un seul objet "
       "masqué, dont l'arrière-plan n'a pas vocation à être reconstruit."),
    ES("Preajuste para capturas que giran alrededor de un único objeto "
       "enmascarado, cuyo fondo no se pretende reconstruir."),
    PT("Predefinição para capturas que giram em torno de um único objeto "
       "mascarado, cujo fundo não se pretende reconstruir."),
    IT("Preimpostazione per riprese che ruotano attorno a un singolo oggetto "
       "mascherato, il cui sfondo non va ricostruito."),
    NL("Voorinstelling voor opnamen die om één gemaskeerd object heen draaien, "
       "waarvan de achtergrond niet gereconstrueerd hoeft te worden."),
    RU("Пресет для съёмок, обходящих один замаскированный "
       "объект, фон которых восстанавливать не требуется."),
    TR("Maskelenmiş tek bir nesnenin çevresinde dönerek yapılan çekimler "
       "için hazır ayar; arka planın yeniden oluşturulması amaçlanmaz."));

SS_MSG(preset_linear_color,
    EN("Linear colour space"),
    JA("リニア色空間"),
    ZH_HANS("线性色彩空间"),
    ZH_HANT("線性色彩空間"),
    KO("선형 색공간"),
    DE("Linearer Farbraum"),
    FR("Espace colorimétrique linéaire"),
    ES("Espacio de color lineal"),
    PT("Espaço de cor linear"),
    IT("Spazio colore lineare"),
    NL("Lineaire kleurruimte"),
    RU("Линейное цветовое пространство"),
    TR("Doğrusal renk uzayı"));

SS_MSG(preset_linear_color_help,
    EN("Preset for training splats in linear color spaces (e.g. ACEScg)."),
    JA("リニア色空間（ACEScg など）でスプラットを学習するためのプリセットです。"),
    ZH_HANS("用于在线性色彩空间（如 ACEScg）中训练泼溅的预设。"),
    ZH_HANT("用於在線性色彩空間（如 ACEScg）中訓練潑濺的預設。"),
    KO("선형 색공간(ACEScg 등)에서 스플랫을 학습하기 위한 프리셋입니다."),
    DE("Voreinstellung für das Training von Splats in linearen Farbräumen "
       "(z. B. ACEScg)."),
    FR("Préréglage pour entraîner des splats dans des espaces colorimétriques "
       "linéaires (ACEScg, par exemple)."),
    ES("Preajuste para entrenar splats en espacios de color lineales (por "
       "ejemplo ACEScg)."),
    PT("Predefinição para treinar splats em espaços de cor lineares (por "
       "exemplo ACEScg)."),
    IT("Preimpostazione per addestrare splat in spazi colore lineari (per "
       "esempio ACEScg)."),
    NL("Voorinstelling om splats te trainen in lineaire kleurruimten "
       "(bijvoorbeeld ACEScg)."),
    RU("Пресет для обучения сплатов в линейных цветовых пространствах "
       "(например, ACEScg)."),
    TR("Splat'ları doğrusal renk uzaylarında (örneğin ACEScg) eğitmek için "
       "hazır ayar."));

SS_MSG(preset_synthetic,
    EN("Synthetic renders"),
    JA("CGレンダリング"),
    ZH_HANS("合成渲染图"),
    ZH_HANT("合成算圖"),
    KO("합성 렌더링"),
    DE("Synthetische Renderings"),
    FR("Rendus de synthèse"),
    ES("Renders sintéticos"),
    PT("Renderizações sintéticas"),
    IT("Render sintetici"),
    NL("Synthetische renders"),
    RU("Синтетические рендеры"),
    TR("Sentetik görüntüler"));

SS_MSG(preset_synthetic_help,
    EN("Preset for training splats on synthetic datasets rendered with "
       "constant exposure."),
    JA("露出を一定にしてレンダリングした合成データセットで学習するためのプリ"
       "セットです。"),
    ZH_HANS("用于在以恒定曝光渲染的合成数据集上训练的预设。"),
    ZH_HANT("用於在以固定曝光算圖的合成資料集上訓練的預設。"),
    KO("노출을 일정하게 해서 렌더링한 합성 데이터셋을 학습하기 위한 "
       "프리셋입니다."),
    DE("Voreinstellung für das Training auf synthetischen Datensätzen, die mit "
       "konstanter Belichtung gerendert wurden."),
    FR("Préréglage pour l'entraînement sur des jeux de données de synthèse "
       "rendus à exposition constante."),
    ES("Preajuste para entrenar con conjuntos de datos sintéticos renderizados "
       "con exposición constante."),
    PT("Predefinição para treinar com conjuntos de dados sintéticos "
       "renderizados com exposição constante."),
    IT("Preimpostazione per addestrare su set di dati sintetici renderizzati a "
       "esposizione costante."),
    NL("Voorinstelling om te trainen op synthetische datasets die met "
       "constante belichting zijn gerenderd."),
    RU("Пресет для обучения на синтетических наборах, отрендеренных с "
       "постоянной экспозицией."),
    TR("Sabit pozlamayla üretilmiş sentetik veri kümelerinde eğitim için hazır "
       "ayar."));

SS_MSG(preset_meshing,
    EN("For meshing"),
    JA("メッシュ化向け"),
    ZH_HANS("用于生成网格"),
    ZH_HANT("用於產生網格"),
    KO("메시 변환용"),
    DE("Für die Netzerzeugung"),
    FR("Pour le maillage"),
    ES("Para generar malla"),
    PT("Para gerar malha"),
    IT("Per la mesh"),
    NL("Voor mesh-generatie"),
    RU("Для построения меша"),
    TR("Ağ (mesh) çıkarmak için"));

SS_MSG(preset_meshing_help,
    EN("Preset for training splats for meshing, aimed at the quality of the "
       "mesh geometry rather than at how the splats themselves look."),
    JA("メッシュ化のためにスプラットを学習するプリセットです。スプラット自体の"
       "見た目よりも、生成されるメッシュ形状の質を重視します。"),
    ZH_HANS("为生成网格而训练泼溅的预设。它看重的是生成网格几何的质量，"
            "而不是泼溅本身的观感。"),
    ZH_HANT("為產生網格而訓練潑濺的預設。它看重的是產生網格幾何的品質，"
            "而不是潑濺本身的觀感。"),
    KO("메시로 만들기 위해 스플랫을 학습하는 프리셋입니다. 스플랫 자체의 "
       "겉모습보다 만들어질 메시 형상의 품질을 우선합니다."),
    DE("Voreinstellung für das Training von Splats zur Netzerzeugung. Sie "
       "zielt auf die Qualität der Netzgeometrie, nicht darauf, wie die Splats "
       "selbst aussehen."),
    FR("Préréglage pour entraîner des splats en vue du maillage. Il vise la "
       "qualité de la géométrie du maillage plutôt que l'aspect des splats "
       "eux-mêmes."),
    ES("Preajuste para entrenar splats con vistas al mallado. Busca la calidad "
       "de la geometría de la malla más que el aspecto de los propios splats."),
    PT("Predefinição para treinar splats visando a malha. Mira a qualidade da "
       "geometria da malha, e não a aparência dos próprios splats."),
    IT("Preimpostazione per addestrare splat in vista della mesh. Punta alla "
       "qualità della geometria della mesh più che all'aspetto degli splat "
       "stessi."),
    NL("Voorinstelling om splats te trainen met het oog op mesh-generatie. Ze "
       "mikt op de kwaliteit van de meshgeometrie, niet op hoe de splats er "
       "zelf uitzien."),
    RU("Пресет для обучения сплатов под построение меша. Он нацелен на "
       "качество геометрии меша, а не на то, как выглядят сами сплаты."),
    TR("Ağ çıkarmak üzere splat eğitmek için hazır ayar. Splat'ların kendi "
       "görünüşünden çok, üretilecek ağ geometrisinin niteliğini gözetir."));

SS_MSG(preset_academic_baseline,
    EN("Academic baseline"),
    JA("論文どおりのベースライン"),
    ZH_HANS("论文基准"),
    ZH_HANT("論文基準"),
    KO("논문 기준선"),
    DE("Akademische Referenz"),
    FR("Référence académique"),
    ES("Línea base académica"),
    PT("Linha de base acadêmica"),
    IT("Riferimento accademico"),
    NL("Academische referentie"),
    RU("Академический эталон"),
    TR("Akademik referans"));

SS_MSG(preset_academic_baseline_help,
    EN("Preset that replicates 3DGS MCMC as faithful as possible."),
    JA("3DGS MCMC をできるかぎり忠実に再現するプリセットです。"),
    ZH_HANS("尽可能忠实复现 3DGS MCMC 的预设。"),
    ZH_HANT("盡可能忠實重現 3DGS MCMC 的預設。"),
    KO("3DGS MCMC를 가능한 한 충실하게 재현하는 프리셋입니다."),
    DE("Voreinstellung, die 3DGS MCMC so getreu wie möglich nachbildet."),
    FR("Préréglage qui reproduit 3DGS MCMC aussi fidèlement que possible."),
    ES("Preajuste que reproduce 3DGS MCMC con la mayor fidelidad posible."),
    PT("Predefinição que reproduz 3DGS MCMC com a maior fidelidade possível."),
    IT("Preimpostazione che riproduce 3DGS MCMC nel modo più fedele "
       "possibile."),
    NL("Voorinstelling die 3DGS MCMC zo getrouw mogelijk nabootst."),
    RU("Пресет, максимально точно воспроизводящий 3DGS MCMC."),
    TR("3DGS MCMC'yi olabildiğince sadık biçimde yeniden üreten hazır ayar."));

// ---------------------------------------------------------------------------
// name -> text. The names are config/TrainConfig.h's kTrainPresets, and each
// consumer static_asserts that the two lists are the same length.
// ---------------------------------------------------------------------------

struct PresetText {
    const char* name;
    const Msg* label;
    const Msg* help;
};

inline constexpr PresetText kPresetText[] = {
    {"3dgs",              &preset_3dgs,              &preset_3dgs_help},
    {"360-camera",        &preset_360_camera,        &preset_360_camera_help},
    {"in-the-wild",       &preset_in_the_wild,       &preset_in_the_wild_help},
    {"centered-object",   &preset_centered_object,   &preset_centered_object_help},
    {"linear-color",      &preset_linear_color,      &preset_linear_color_help},
    {"synthetic",         &preset_synthetic,         &preset_synthetic_help},
    {"meshing",           &preset_meshing,           &preset_meshing_help},
    // hidden by default, uncomment to enable
   //  {"academic-baseline", &preset_academic_baseline, &preset_academic_baseline_help},
};
inline constexpr size_t kNumPresetText =
    sizeof(kPresetText) / sizeof(kPresetText[0]);

// Null for a name that has no entry -- callers fall back to the name itself,
// so a preset added to TrainConfig.h without text here still works.
inline const PresetText* preset_text(const char* name) {
    for (const PresetText& p : kPresetText)
        if (std::strcmp(p.name, name) == 0) return &p;
    return nullptr;
}


// ===========================================================================
// Section headings -- the `section` column of config/TrainConfig.h's field
// table, shown by `spirula train --help` and by the GUI's options editor.
// ===========================================================================

SS_MSG(section_run,
    EN("Run & Output"),  JA("実行と出力"),     ZH_HANS("运行与输出"), ZH_HANT("執行與輸出"),
    KO("실행과 출력"),    DE("Lauf & Ausgabe"), FR("Exécution et sortie"),
    ES("Ejecución y salida"), PT("Execução e saída"), IT("Esecuzione e output"),
    NL("Run en uitvoer"), RU("Запуск и вывод"), TR("Çalıştırma ve çıktı"));

SS_MSG(section_dataset,
    EN("Dataset"),       JA("データセット"),   ZH_HANS("数据集"),   ZH_HANT("資料集"),
    KO("데이터셋"),       DE("Datensatz"),    FR("Jeu de données"),
    ES("Conjunto de datos"), PT("Conjunto de dados"), IT("Set di dati"),
    NL("Dataset"),       RU("Набор данных"), TR("Veri kümesi"));

SS_MSG(section_scene,
    EN("Scene Placement"), JA("シーンの位置と向き"), ZH_HANS("场景位置"),
    ZH_HANT("場景位置"),  KO("장면 위치"),     DE("Szenenausrichtung"),
    FR("Placement de la scène"), ES("Ubicación de la escena"),
    PT("Posicionamento da cena"), IT("Posizione della scena"),
    NL("Scèneplaatsing"), RU("Размещение сцены"), TR("Sahne yerleşimi"));

SS_MSG(section_splats,
    EN("Splat Model"),   JA("スプラットのモデル"), ZH_HANS("泼溅模型"), ZH_HANT("潑濺模型"),
    KO("스플랫 모델"),    DE("Splat-Modell"), FR("Modèle de splats"),
    ES("Modelo de splats"), PT("Modelo de splats"), IT("Modello di splat"),
    NL("Splatmodel"),    RU("Модель сплатов"), TR("Splat modeli"));

SS_MSG(section_detail,
    EN("Detail & Splat Count"),
    JA("精細さとスプラット数"),
    ZH_HANS("细节与泼溅数"),
    ZH_HANT("細節與潑濺數"),
    KO("디테일과 스플랫 수"),
    DE("Detail & Splat-Anzahl"),
    FR("Détail et nombre de splats"),
    ES("Detalle y número de splats"),
    PT("Detalhe e número de splats"),
    IT("Dettaglio e numero di splat"),
    NL("Detail en aantal splats"),
    RU("Детализация и число сплатов"),
    TR("Ayrıntı ve splat sayısı"));

SS_MSG(section_loss,
    EN("Image Loss"),    JA("画像の損失"),     ZH_HANS("图像损失"),  ZH_HANT("影像損失"),
    KO("이미지 손실"),    DE("Bildverlust"),  FR("Perte d'image"),
    ES("Pérdida de imagen"), PT("Perda de imagem"), IT("Perdita d'immagine"),
    NL("Beeldverlies"),  RU("Потери по изображению"), TR("Görüntü kaybı"));

SS_MSG(section_geometry,
    EN("Geometry & Surfaces"),
    JA("ジオメトリと面"),
    ZH_HANS("几何与表面"),
    ZH_HANT("幾何與表面"),
    KO("지오메트리와 표면"),
    DE("Geometrie & Oberflächen"),
    FR("Géométrie et surfaces"),
    ES("Geometría y superficies"),
    PT("Geometria e superfícies"),
    IT("Geometria e superfici"),
    NL("Geometrie en oppervlakken"),
    RU("Геометрия и поверхности"),
    TR("Geometri ve yüzeyler"));

SS_MSG(section_shape,
    EN("Splat Shape"),   JA("スプラットの形状"), ZH_HANS("泼溅形状"), ZH_HANT("潑濺形狀"),
    KO("스플랫 모양"),    DE("Splat-Form"),   FR("Forme des splats"),
    ES("Forma de los splats"), PT("Forma dos splats"), IT("Forma degli splat"),
    NL("Splatvorm"),     RU("Форма сплатов"), TR("Splat biçimi"));

SS_MSG(section_correction,
    EN("Camera & Color Correction"),
    JA("カメラと色の補正"),
    ZH_HANS("相机与色彩校正"),
    ZH_HANT("相機與色彩校正"),
    KO("카메라와 색 보정"),
    DE("Kamera- & Farbkorrektur"),
    FR("Correction caméra et couleur"),
    ES("Corrección de cámara y color"),
    PT("Correção de câmera e cor"),
    IT("Correzione camera e colore"),
    NL("Camera- en kleurcorrectie"),
    RU("Коррекция камеры и цвета"),
    TR("Kamera ve renk düzeltme"));

SS_MSG(section_colorspace,
    EN("Color Space"),   JA("色空間"),        ZH_HANS("色彩空间"),  ZH_HANT("色彩空間"),
    KO("색 공간"),        DE("Farbraum"),     FR("Espace colorimétrique"),
    ES("Espacio de color"), PT("Espaço de cor"), IT("Spazio colore"),
    NL("Kleurruimte"),   RU("Цветовое пространство"), TR("Renk uzayı"));

SS_MSG(section_perf,
    EN("Speed & Memory"), JA("速度とメモリ"),   ZH_HANS("速度与内存"), ZH_HANT("速度與記憶體"),
    KO("속도와 메모리"),   DE("Geschwindigkeit & Speicher"), FR("Vitesse et mémoire"),
    ES("Velocidad y memoria"), PT("Velocidade e memória"), IT("Velocità e memoria"),
    NL("Snelheid en geheugen"), RU("Скорость и память"), TR("Hız ve bellek"));

SS_MSG(section_rates,
    EN("Learning Rates"), JA("学習率"),        ZH_HANS("学习率"),   ZH_HANT("學習率"),
    KO("학습률"),         DE("Lernraten"),    FR("Taux d'apprentissage"),
    ES("Tasas de aprendizaje"), PT("Taxas de aprendizado"),
    IT("Tassi di apprendimento"), NL("Leersnelheden"),
    RU("Скорости обучения"), TR("Öğrenme oranları"));

// ---------------------------------------------------------------------------
// name -> heading. The names are config/TrainConfig.h's kTrainSections, and
// each consumer static_asserts that the two lists are the same length.
// ---------------------------------------------------------------------------

struct SectionText {
    const char* name;
    const Msg* label;
};

inline constexpr SectionText kSectionText[] = {
    {"run",        &section_run},
    {"dataset",    &section_dataset},
    {"scene",      &section_scene},
    {"splats",     &section_splats},
    {"detail",     &section_detail},
    {"loss",       &section_loss},
    {"geometry",   &section_geometry},
    {"shape",      &section_shape},
    {"correction", &section_correction},
    {"colorspace", &section_colorspace},
    {"perf",       &section_perf},
    {"rates",      &section_rates},
};
inline constexpr size_t kNumSectionText =
    sizeof(kSectionText) / sizeof(kSectionText[0]);

// Null for a heading with no entry, so a section added to TrainConfig.h
// without text here still lists its flags -- under its bare name.
inline const Msg* section_label(const char* name) {
    for (const SectionText& s : kSectionText)
        if (std::strcmp(s.name, name) == 0) return s.label;
    return nullptr;
}

}  // namespace train
}  // namespace msg
}  // namespace i18n
}  // namespace spirula

#include "i18n/EndCatalog.h"
