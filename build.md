Building vag-blocks (based on ubuntu 20.04)

```bash

sudo apt-get install build-essential subversion git libudev-dev libcanberra-gtk-module

sudo add-apt-repository ppa:rock-core/qt4
sudo apt-get install qt4-dev-tools




mkdir vag-blocks-install
cd vag-blocks-install
git clone https://github.com/jazdw/vag-blocks


git clone git://code.qt.io/qt/qtserialport.git

echo checkout old version of qtserialport
cd qtserialport
git checkout qt4-dev
git checkout 98f7762ea7268ad751607ec45fb22b03472643a5
cd ..

mkdir qtserialport-build
cd qtserialport-build
qmake-qt4 ../qtserialport/serialport.pro
make
sudo make install
cd ..

svn export svn://svn.code.sf.net/p/qwt/code/branches/qwt-6.0
cd qwt-6.0/
qmake-qt4 qwt.pro
make
sudo make install
sudo cp -a cd /usr/local/qwt-6.0.3-svn/lib/* /usr/lib/.
sudo ldconfig
cd ..


cd vag-blocks
qmake-qt4 vag-blocks.pro
make

#start program
./vagblocks

```
