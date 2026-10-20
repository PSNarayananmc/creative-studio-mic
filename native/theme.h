#pragma once
#include <QApplication>
inline void applyTheme(QApplication &application) {
    application.setStyle("Fusion");
    application.setProperty("theme", "dark");
    application.setStyleSheet(R"CSS(
 QWidget{background:#11141d;color:#e3e8f2;font-family:DejaVu Sans;font-size:12px;}
 QMainWindow,QTabWidget::pane{background:#151a26;}QTabWidget::pane{border:1px solid #2a3245;border-radius:8px;}
 QTabBar::tab{background:#11141d;color:#8b95aa;padding:12px 24px;border-bottom:2px solid transparent;}QTabBar::tab:selected{color:#b5f2cc;border-bottom:2px solid #a8f0c6;}
 QLabel{background:transparent;}QLabel#heading{font-size:20px;font-weight:600;color:#f0e6cb;}QLabel#muted{color:#8792a8;font-size:11px;}
 QPushButton{background:#242c3d;border:1px solid #36405a;border-radius:6px;padding:10px 16px;}QPushButton:hover{background:#33405a;}QPushButton:pressed{background:#435573;}QPushButton#start{background:#a8f0c6;color:#12231b;border:0;font-size:22px;font-weight:bold;}QPushButton#start:hover{background:#c8ffdd;}
 QComboBox,QDoubleSpinBox{background:#1c2333;border:1px solid #36405a;border-radius:5px;padding:8px;min-height:18px;}QComboBox QAbstractItemView{background:#242c3d;selection-background-color:#435573;}
 QProgressBar{border:0;background:#303849;border-radius:3px;}QProgressBar::chunk{background:#a8f0c6;border-radius:3px;}
 QCheckBox{spacing:8px;padding:6px;}QCheckBox::indicator{width:16px;height:16px;}QScrollArea{border:0;}QMessageBox QPushButton{min-width:75px;}
 )CSS");
}
