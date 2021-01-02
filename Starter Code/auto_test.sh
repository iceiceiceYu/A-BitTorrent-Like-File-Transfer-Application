# 18302010018 Yu Zhexuan
# Auto Test checkpoint1, checkpoint2, checkpoint3 and concurrent test

cd ./starter_code;
make clean;
make;
cp peer ../cp1;
cp peer ../cp2;
cp peer ../cp3;
cp peer ../concurrenttest;

cd ../cp1;
echo -e '\n---------- NOW testing checkpoint1 ----------';
ruby checkpoint1.rb;

cd ../cp2;
echo -e '\n---------- NOW testing checkpoint2 ----------';
ruby checkpoint2.rb;

cd ../cp3;
echo -e '\n---------- NOW testing checkpoint3 ----------';
ruby checkpoint3.rb;

cd ../concurrenttest;
echo -e '\n---------- NOW testing concurrenttest ----------';
ruby concurrenttest.rb;