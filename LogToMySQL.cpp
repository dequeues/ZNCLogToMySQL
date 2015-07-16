#include <znc/znc.h>
#include <znc/Modules.h>
#include <znc/Chan.h>
#include <znc/IRCNetwork.h>
#include <znc/User.h>
 
#include <time.h>
#include <stdlib.h>
#include <iostream>
#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include <map>


using namespace std;

class CMySQLLog : public CModule {
public:
	MODCONSTRUCTOR(CMySQLLog) {}

	virtual bool OnLoad(const CString& sArgsi, CString& sMessage) override
	{
		CString configOK = SetConfigFromArgs(sArgsi);

		if (configOK != "success")
		{
			sMessage = configOK;
			return false;
		}
		sMessage = "Success!";
		Connect();
		return true;
	}

	CString SetConfigFromArgs(CString sArgsi)
	{
		map<CString, CString> config;
		VCString args;
		sArgsi.Split(";", args);
		vector<CString> requiredconfigitems = {"database", "host", "username", "password"};

		for (unsigned int i = 0; i < args.size(); i++)
		{
			CString settingname;
			CString setting;
			settingname = args[i].Token(0, false, ":");
			setting = args[i].Token(1, true, ":");
			if (settingname != "" && setting != "")
			{
				conInfo[settingname] = setting;
			}
		}

		for (vector<CString>::iterator it = requiredconfigitems.begin(); it != requiredconfigitems.end(); ++it)
		{
			if (conInfo.find(*it) == conInfo.end())
			{
				return CString("'" + *it + "' not found");
			}
		}
		return "success";
	}

	EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override {
		map<CString, CString> params;
		params["type"] = "chan";
		params["channel"] = Channel.GetName();
		params["nick"] = Nick.GetNick();
		params["identhost"] = Nick.GetIdent() + "@" + Nick.GetHost();
		params["message"] = sMessage;
		InsertToDB(params);

		return CONTINUE;
	}

	EModRet OnPrivMsg(CNick& Nick, CString& sMessage) override {
		//"INSERT INTO chatlogs (znc_user, type, sent_to, nick, identhost, timestamp, message) VALUES (?, ?, ?, ?, ?, ?, ?)";
		map<CString, CString> params;
		params["type"] = "privmsg";
		params["identhost"] = Nick.GetIdent() + "@" + Nick.GetHost();
		params["message"] = sMessage;
		params["nick"] = Nick.GetNick();
		InsertToDB(params);
		return CONTINUE;
	}

	void OnJoin(const CNick& Nick, CChan& Channel) override {
		map<CString, CString> params;
		params["type"] = "chan";
		params["channel"] = Channel.GetName();
		params["nick"] = Nick.GetNick();
		params["identhost"] = Nick.GetIdent() + "@" + Nick.GetHost();
		params["message"] = "*** Joins: " + Nick.GetNick() + " (" + CString(Nick.GetIdent() + "@" + Nick.GetHost()) + ")";
		InsertToDB(params);
	}

	void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage)
	{
		map<CString, CString> params;
		params["type"] = "chan";
		params["channel"] = Channel.GetName();
		params["nick"] = sKickedNick;
		params["message"] = "*** " + sKickedNick + " was kicked by " + OpNick.GetNick() + " (" + sMessage + ")";
		InsertToDB(params);
	}

	void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans)
	{
		map<CString, CString> params;
		params["type"] = "chan";
		for (vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it)
		{
			params["channel"] = (*it)->GetName();
			params["nick"] = Nick.GetNick();
			params["identhost"] = Nick.GetIdent() + "@" + Nick.GetHost();
			params["message"] = "*** Quits: " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ") (" + sMessage + ")";
			InsertToDB(params);
		}
	}

	void OnNick(const CNick& OldNick, const CString& sNewNick, const vector<CChan*>& vChans)
	{
		map<CString, CString> params;
		params["type"] = "chan";
		for (vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it)
		{
			params["channel"] = (*it)->GetName();
			params["nick"] = sNewNick;
			params["identhost"] = OldNick.GetIdent() + "@" + OldNick.GetHost();
			params["message"] = "*** " + OldNick.GetNick() + " is now known as " + sNewNick;
			InsertToDB(params);
		}
	}

	void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage)
	{
		map<CString, CString> params;
		params["type"] = "chan";
		params["channel"] = Channel.GetName();
		params["nick"] = Nick.GetNick();
		params["identhost"] = Nick.GetIdent() + "@" + Nick.GetHost();
		params["message"] = "*** Parts: " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ") (" + sMessage + ")";
		InsertToDB(params);
	}

	EModRet OnUserNotice(CString& sTarget, CString& sMessage)
	{
		CIRCNetwork* pNetwork = GetNetwork();
		if (pNetwork)
		{
			map<CString, CString> params;
			params["type"] = "notice";
			params["nick"] = pNetwork->GetCurNick();
			params["message"] = "->" + sTarget + " - " + pNetwork->GetCurNick() + "- " + sMessage;
			InsertToDB(params);
		}

		return CONTINUE;
	}

	EModRet OnPrivNotice(CNick& Nick, CString& sMessage)
	{
		map<CString, CString> params;
		params["type"] = "notice";
		params["nick"] = Nick.GetNick();
		params["identhost"] = Nick.GetIdent() + "@" + Nick.GetHost();
		params["message"] = "-" + Nick.GetNick() + "- " + sMessage;
		InsertToDB(params);

		return CONTINUE;
	}

	EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage)
	{
		map<CString, CString> params;
		params["type"] = "notice";
		params["channel"] = Channel.GetName();
		params["nick"] = Nick.GetNick();
		params["identhost"] = Nick.GetIdent() + "@" + Nick.GetHost();
		params["message"] = "-" + Nick.GetNick() + "- " + sMessage;
		InsertToDB(params);

		return CONTINUE;
	}

	void OnRawMode2(const CNick* pOpNick, CChan& Channel, const CString& sModes, const CString& sArgs)
	{
		map<CString, CString> params;

		CString theNick;
		CString identhost;

		CString sNickMask = pOpNick ? pOpNick->GetNickMask() : "Server";

		if (pOpNick)
		{
			theNick = pOpNick->GetNick();
			identhost = pOpNick->GetIdent() + "@" + pOpNick->GetHost();
		} else {
			theNick = "Server";
			identhost = "server@server";
		}

		params["type"] = "chan";
		params["channel"] = Channel.GetName();
		params["nick"] = theNick;
		params["identhost"] = identhost;
		params["message"] = "*** " + theNick + " sets mode: " + sModes + " " + sArgs;
		InsertToDB(params);
	}

	EModRet OnTopic(CNick& Nick, CChan& Channel, CString& sTopic)
	{
		map<CString, CString> params;
		params["type"] = "chan";
		params["channel"] = Channel.GetName();
		params["nick"] = Nick.GetNick();
		params["identhost"] = Nick.GetIdent() + "@" + Nick.GetHost();
		params["message"] = "*** " + Nick.GetNick() + " changes topic to '" + sTopic + "'";
		InsertToDB(params);

		return CONTINUE;
	}

	virtual EModRet OnModuleUnloading(CModule* pModule, bool& bSuccess, CString& sRetMsg)
	{
		delete stmt;
		delete res;
		try {
			con->close();
			delete con;
		} catch (sql::SQLException &e) { }
		return CONTINUE;
	}

	virtual void InsertToDB(map<CString, CString> params)
	{

		if (!connected)
		{
			PutModule("Not connected");
			return;
		}


		CString query = "INSERT INTO chatlogs (znc_user, type, channel, nick, identhost, timestamp, message) VALUES (?, ?, ?, ?, ?, ?, ?)";
		params["znc_user"] = GetUser()->GetUserName();
		params["timestamp"] = CString(time(NULL));

		vector<CString> queryOrder = {"znc_user", "type", "channel", "nick", "identhost", "timestamp", "message"};

		for (vector<CString>::iterator it = queryOrder.begin(); it != queryOrder.end(); ++it)
		{
			if (params.find(*it) == params.end())
			{
				params[*it] = "";
			}
		}

		try {
			prep_stmt = con->prepareStatement(query);
			unsigned int i = 1;
			for (vector<CString>::iterator it = queryOrder.begin(); it != queryOrder.end(); ++it)
			{
				prep_stmt->setString(i, params[(*it)]);
				i++;
			}
			prep_stmt->execute();
		} catch (sql::SQLException &e) {
			PutModule(CString(e.what()));
		}
	}

	virtual void Connect()
	{
		if (tries == 3)
		{
			return;
		}

		try {
			PutModule("connecting to " + conInfo["host"]);
			driver = get_driver_instance();
			con = driver->connect(conInfo["host"], conInfo["username"], conInfo["password"]);
			con->setSchema(conInfo["database"]);
			connected = true;
		} catch (sql::SQLException &e) {
			PutModule(CString(e.getErrorCode()));
			tries++;
		}
	}

private:
	sql::Driver *driver;
	sql::Connection *con;
	sql::Statement *stmt;
	sql::ResultSet *res;
	sql::PreparedStatement *prep_stmt;
	map<CString, CString> conInfo;
	bool connected = false;
	int tries = 0;
};

template<> void TModInfo<CMySQLLog>(CModInfo& Info) {
	Info.SetWikiPage("");
}

USERMODULEDEFS(CMySQLLog, "Log to MySQL");