if not exist asio git clone https://github.com/chriskohlhoff/asio.git
rem cl /Zi /EHsc /Od /MDd /wd4503 test\test_all.cpp detail\SpookyV2.cpp /DUNICODE=1 /Iinclude /Itest /DAFIO_STANDALONE=1 /Iasio/asio/include /DSPINLOCK_STANDALONE=1 /DASIO_STANDALONE=1 /DBOOST_TEST_DYN_LINK=1
rem async_io_enumerate segfaults
rem async_io_errors fails with hasErrorDirectly = 2 and hasErrorFromBarrier = 2
rem test_all.exe async_io_lstat_works -r xml -o results.xml
rem cl /Zi /EHsc /Od /MDd /wd4503 test\test_all.cpp detail\SpookyV2.cpp /DUNICODE=1 /Iinclude /Itest /DAFIO_STANDALONE=1 /Iasio/asio/include /DSPINLOCK_STANDALONE=1 /DASIO_STANDALONE=1 /DBOOST_TEST_DYN_LINK=1 /I..\boost-release /link /LIBPATH:..\boost-release\stage\lib
rem cl /Zi /EHs /O2 /MD /openmp /GF /GR /Gy /bigobj /wd4503 /Zc:forScope /Zc:wchar_t /W3 test\test_all.cpp detail\SpookyV2.cpp /DUNICODE=1 /DWIN32=1 /D_UNICODE=1 /D_WIN32=1 /Iinclude /Itest /DAFIO_STANDALONE=1 /Iasio/asio/include /DSPINLOCK_STANDALONE=1 /DBOOST_AFIO_RUNNING_IN_CI=1 /DBOOST_AFIO_USE_BOOST_UNIT_TEST=1  /I..\boost-release /link  /LIBPATH:..\boost-release\stage\lib /SUBSYSTEM:CONSOLE
cl /Z7 /EHs /O2 /MD /openmp /GF /GR /Gy /bigobj /wd4503 /Zc:forScope /Zc:wchar_t /W3 test\test_all.cpp detail\SpookyV2.cpp /DUNICODE=1 /DWIN32=1 /D_UNICODE=1 /D_WIN32=1 /Iinclude /Itest /Iasio/asio/include /DBOOST_AFIO_RUNNING_IN_CI=1 /DBOOST_AFIO_USE_BOOST_UNIT_TEST=1  /I..\boost-release /link  /LIBPATH:..\boost-release\stage\lib /SUBSYSTEM:CONSOLE
