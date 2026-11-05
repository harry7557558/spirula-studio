#pragma once

#include "i18n/BeginCatalog.h"
namespace spirula::i18n::msg::background_export {

SS_MSG(summary,
    EN("export an equirectangular SH background"),
    JA("SH背景を正距円筒画像として出力"),
    ZH_HANS("导出等距柱状投影 SH 背景"), ZH_HANT("匯出等距柱狀投影 SH 背景"),
    KO("SH 배경을 정거리 원통형 이미지로 내보내기"),
    DE("SH-Hintergrund als equirektangulares Bild exportieren"),
    FR("exporter un arrière-plan SH équirectangulaire"),
    ES("exportar un fondo SH equirectangular"), PT("exportar um fundo SH equiretangular"),
    IT("esporta uno sfondo SH equirettangolare"), NL("een equirectangulaire SH-achtergrond exporteren"),
    RU("экспорт фона SH в равнопромежуточной проекции"), TR("SH arka planını eşit aralıklı silindirik görüntü olarak dışa aktar"));
SS_MSG(usage,
    EN("Usage: {0}"), JA("使用方法: {0}"), ZH_HANS("用法：{0}"), ZH_HANT("用法：{0}"),
    KO("사용법: {0}"), DE("Verwendung: {0}"), FR("Utilisation : {0}"), ES("Uso: {0}"),
    PT("Uso: {0}"), IT("Uso: {0}"), NL("Gebruik: {0}"), RU("Использование: {0}"), TR("Kullanım: {0}"));
SS_MSG(saved,
    EN("Background exported to {0}"), JA("背景を {0} に出力しました"),
    ZH_HANS("背景已导出到 {0}"), ZH_HANT("背景已匯出至 {0}"), KO("배경을 {0}에 내보냈습니다"),
    DE("Hintergrund nach {0} exportiert"), FR("Arrière-plan exporté vers {0}"),
    ES("Fondo exportado a {0}"), PT("Fundo exportado para {0}"), IT("Sfondo esportato in {0}"),
    NL("Achtergrond geëxporteerd naar {0}"), RU("Фон экспортирован в {0}"), TR("Arka plan {0} konumuna aktarıldı"));
SS_MSG(failed,
    EN("Could not export background from {0} to {1}. Check the SH checkpoint and output path."),
    JA("{0} から {1} に背景を出力できません。SHチェックポイントと出力先を確認してください。"),
    ZH_HANS("无法将背景从 {0} 导出到 {1}。请检查 SH 检查点和输出路径。"),
    ZH_HANT("無法將背景從 {0} 匯出至 {1}。請檢查 SH 檢查點和輸出路徑。"),
    KO("{0}에서 {1}로 배경을 내보낼 수 없습니다. SH 체크포인트와 출력 경로를 확인하세요."),
    DE("Hintergrund konnte nicht von {0} nach {1} exportiert werden. SH-Checkpoint und Ausgabepfad prüfen."),
    FR("Impossible d'exporter l'arrière-plan de {0} vers {1}. Vérifiez le checkpoint SH et le chemin de sortie."),
    ES("No se pudo exportar el fondo de {0} a {1}. Compruebe el checkpoint SH y la ruta de salida."),
    PT("Não foi possível exportar o fundo de {0} para {1}. Verifique o checkpoint SH e o caminho de saída."),
    IT("Impossibile esportare lo sfondo da {0} a {1}. Controllare il checkpoint SH e il percorso di uscita."),
    NL("Kan de achtergrond niet van {0} naar {1} exporteren. Controleer het SH-checkpoint en het uitvoerpad."),
    RU("Не удалось экспортировать фон из {0} в {1}. Проверьте контрольную точку SH и путь вывода."),
    TR("Arka plan {0} konumundan {1} konumuna aktarılamadı. SH kontrol noktasını ve çıktı yolunu kontrol edin."));

}
#include "i18n/EndCatalog.h"
