#ifndef credentialhandler_h
#define credentialhandler_h

#include <WString.h>
#include <string>

class CredentialHandler {
public:
  CredentialHandler();
  ~CredentialHandler();

  const char* getWifiHost();
  const char* getWifiSsid();
  const char* getWifiPass();
  const char* getApSsid();
  const char* getApPass();

  void setWifiHost(std::string t_wifiHost);
  void setWifiSsid(std::string t_wifiSsid);
  void setWifiPass(std::string t_wifiPass);
  void setApSsid(std::string t_apSsid);
  void setApPass(std::string t_apPass);
  void useDefaults();
private:
  std::string m_wifiHost;
  std::string m_wifiSsid;
  std::string m_wifiPass;

  std::string m_apSsid;
  std::string m_apPass;

  void _retrieveCredentials();
  void _persistCredentials();
};

#endif