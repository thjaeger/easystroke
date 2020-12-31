const char *desktop_file = "\
[Desktop Entry]\n\
Version=1.0\n\
Name=Easystroke Gesture Recognition\n\
Type=Application\n\
Terminal=false\n\
Exec=%1$s\n\
Icon=easystroke\n\
Categories=GTK;Utility;Accessibility;\n\
Actions=About;Enable;Disable;Quit\n\
Comment=Control your desktop using mouse gestures\n\
\n\
[Desktop Action About]\n\
Name=About\n\
Exec=%1$s about\n\
\n\
[Desktop Action Enable]\n\
Name=Enable\n\
Exec=%1$s enable\n\
\n\
[Desktop Action Disable]\n\
Name=Disable\n\
Exec=%1$s disable\n\
\n\
[Desktop Action Quit]\n\
Name=Quit\n\
Exec=%1$s quit\n\
";
