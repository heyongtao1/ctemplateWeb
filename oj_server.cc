#include <string>
#include <vector>
#include "cpphttplib/httplib.h"
#include <jsoncpp/json/json.h>
#include "compile.hpp"
#include "util.hpp"
#include "oj_model.hpp"
#include "oj_view.hpp"
#include "connect_pool.h"
#include "mysqlhelper.h"
#include <stdio.h>
#include "cJSON.h"
#include <sys/time.h>
#define NUMBER 20

using namespace mysqlhelper;

void startMySqlConnectPool()
{
	//创建连接池中的连接个数
	vector<MysqlHelper *> m_databases;
	for(int i=0;i<NUMBER;i++){
		printf("new MysqlHelper %d\n",i);
		MysqlHelper *mysqlHelper = new MysqlHelper();
		mysqlHelper->init("172.22.63.3","hyt","123456","cpus_uni_wall","utf8");//uniLJob
		mysqlHelper->connect();
		m_databases.push_back(mysqlHelper);
	}
	
	if(!Singleton<connect_pool>::getInstance().init(m_databases)){
		printf("connect_pool init fail\n");
	}
	else
		printf("mysql connect_pool start success\n");
}

static uint64_t now_sec()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec;
}

bool resigest_noteUser(const string& username , const string& password)
{
	int index = Singleton<connect_pool>::getInstance().get_connect_index();
	
	MysqlHelper* mysqlcon = Singleton<connect_pool>::getInstance().get_connect(index);
	//读写锁
	pthread_rwlock_t rwlock;
	//初始化锁
	pthread_rwlock_init(&rwlock, NULL);
	pthread_rwlock_wrlock(&rwlock);//请求写锁
	//插入blog数据库
	MysqlHelper::RECORD_DATA record;
    int create_time = now_sec();
	record.insert(make_pair("user_account",make_pair(MysqlHelper::DB_STR,username)));
	record.insert(make_pair("user_password",make_pair(MysqlHelper::DB_STR,password)));
    record.insert(make_pair("create_time",make_pair(MysqlHelper::DB_INT,std::to_string(create_time))));
	Singleton<connect_pool>::getInstance().get_connect(index)->insertRecord("user_info",record);

	pthread_rwlock_unlock(&rwlock);//解锁

	Singleton<connect_pool>::getInstance().remove_connect_from_pool(index);
	return true;
}

// controller 作为服务器核心逻辑
// 创建服务器框架，组织逻辑
int main()
{
	//开启数据库连接池
	startMySqlConnectPool();

    OjModel model;
    model.Load();

    using namespace httplib;
    Server server;
    server.Get("/all_questions", [&model](const Request& req,
          Response& resp){
          std::vector<Question> all_questions;
          model.GetAllQuestions(&all_questions);
          std::string html;
          OjView::RenderAllQuestions(all_questions, &html);
          resp.set_content(html, "text/html");
        });
	//login
	server.Get("/login", [&model](const Request& req,
          Response& resp){
          std::string html;
          OjView::RenderLogin(&html);
          resp.set_content(html, "text/html");
        });
		
    server.Get(R"(/question/(\d+))", [&model](const Request& req,
          Response& resp){
          Question question;
          model.GetQuestion(req.matches[1].str(), &question);
          std::string html;
          OjView::RenderQuestion(question, &html);
          resp.set_content(html, "text/html");

        });
    server.Post(R"(/compile/(\d+))", [&model](const Request& req,
          Response& resp){
          //根据id获取到题目信息
          Question question;
          model.GetQuestion(req.matches[1].str(), &question);
          
          //解析body，获取到用户提交的代码
          std::unordered_map<std::string, std::string> body_kv;
          UrlUtil::ParseBody(req.body, &body_kv);
          const std::string& user_code = body_kv["code"];

          //构造json结构的参数
          Json::Value req_json;
          req_json["code"] = user_code + question.tail_cpp;
          req_json["stdin"] = user_code;
          Json::Value resp_json;
          //调用编译模块进行编译
          Compiler::CompileAndRun(req_json, &resp_json);
          //根据编译结果构造网页
          std::string html;
          OjView::RenderResult(resp_json["stdout"].asString(),
              resp_json["reason"].asString(), &html);
          resp.set_content(html, "text/html");
        });
		
	//验证登录 loginResult
	server.Post(R"(/loginResult)", [&model](const Request& req,
          Response& resp){
		  //解析包体获取用户名和密码
		  std::unordered_map<std::string, std::string> body_kv;
          UrlUtil::ParseBody(req.body, &body_kv);
          const std::string& user_name = body_kv["username"];
		  const std::string& pass_word = body_kv["password"];
		  std::cout << user_name << std::endl;
		  std::cout << pass_word << std::endl;
		  std::string html;
		  std::string result;
		  //注册
		  if(resigest_noteUser(user_name,pass_word))
          {
             result = "register success!";
          }
		  else	result = "login fail";
		  OjView::RenderLoginResult(result,&html);
		  resp.set_content(html, "text/html");
		});
    server.set_mount_point("/", "./wwwshy");
    server.listen("0.0.0.0", 8081);
    return 0;
}
