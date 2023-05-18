oj_server:oj_server.cc oj_model.hpp oj_view.hpp compile.hpp util.hpp cJSON.h connect_pool.h mysqlhelper.h Singleton.h
	g++ cJSON.c mysqlhelper.c oj_server.cc -o oj_server -std=c++11 -I ~/third_part/include -lpthread -ljsoncpp -lctemplate -L ~/third_part/lib  -no-pie -L/usr/lib/mysql -lmysqlclient


.PHONY:clean
clean:
	rm oj_server
