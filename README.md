#Compile

g++ main.cpp -llept -ltesseract $(pkg-config --cflags --libs opencv4)
./a.out 'image' 1
./a.out 'video' 2

python3 gui.py
