#include "CredentialHandler.h"

#include <SPIFFS.h>
#include "creds.h"

const String credsFilename = "/credentials";

CredentialHandler::CredentialHandler() {
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to initialize SPIFFS");
  }

  m_wifiHost = "";
  m_wifiSsid = "";
  m_wifiPass = "";
  m_apSsid = "";
  m_apPass = "";

  _retrieveCredentials();
}

CredentialHandler::~CredentialHandler() {}

#pragma region Getters
const char* CredentialHandler::getWifiHost() {
  return m_wifiHost.c_str();
}

const char* CredentialHandler::getWifiSsid() {
  return m_wifiSsid.c_str();
}

const char* CredentialHandler::getWifiPass() {
  return m_wifiPass.c_str();
}

const char* CredentialHandler::getApSsid() {
  return m_apSsid.c_str();
}

const char* CredentialHandler::getApPass() {
  return m_apPass.c_str();
}
#pragma endregion

#pragma region Setters
void CredentialHandler::setWifiHost(std::string t_wifiHost) {
  m_wifiHost = t_wifiHost;
  _persistCredentials();
}

void CredentialHandler::setWifiSsid(std::string t_wifiSsid) {
  m_wifiSsid = t_wifiSsid;
  _persistCredentials();
}

void CredentialHandler::setWifiPass(std::string t_wifiPass) {
  m_wifiPass = t_wifiPass;
  _persistCredentials();
}

void CredentialHandler::setApSsid(std::string t_apSsid) {
  Serial.println("Before set");
  m_apSsid = t_apSsid;
  Serial.println("Before persist");
  Serial.println(m_apSsid.c_str());
  _persistCredentials();
}

void CredentialHandler::setApPass(std::string t_apPass) {
  m_apPass = t_apPass;
  _persistCredentials();
}

void CredentialHandler::useDefaults() {
  m_wifiHost = WIFI_HOST;
  m_wifiSsid = WIFI_SSID;
  m_wifiPass = WIFI_PASSWORD;
  m_apSsid = AP_SSID;
  m_apPass = AP_PASSWORD;
  _persistCredentials();
  Serial.println("Using default WIFI credentials.");
}
#pragma endregion

void CredentialHandler::_retrieveCredentials() {
  File file = SPIFFS.open(credsFilename);

  if (!file) {
    Serial.println("Unable to retrieve credentials file.");
    useDefaults();
    return;
  }

  auto fileSize = file.size();
  char buf[128];

  std::string* v[5] = {
    &m_wifiHost,
    &m_wifiSsid,
    &m_wifiPass,
    &m_apSsid,
    &m_apPass
  };

  int i = 0;
  while (file.available() && i < 5) {
    int l = file.readBytesUntil('\n', buf, sizeof(buf));
    buf[l-1] = 0;  // Replace the last char (which is \n) with null terminator
    // *v[i] = String(buf);
    // String temp(buf);
    // memcpy(v[i], &temp, l);
    *(v[i]) = buf;
    i++;
  }

  Serial.println("Done retrieve");
  Serial.printf("%d %s.\n", m_wifiHost.length(), m_wifiHost.c_str());
  Serial.printf("%d %s.\n", m_wifiSsid.length(), m_wifiSsid.c_str());
  Serial.printf("%d %s.\n", m_wifiPass.length(), m_wifiPass.c_str());
  Serial.printf("%d %s.\n", m_apSsid.length(), m_apSsid.c_str());
  Serial.printf("%d %s.\n", m_apPass.length(), m_apPass.c_str());

  file.close();
}

void CredentialHandler::_persistCredentials() {
  // File file = SPIFFS.open(credsFilename, FILE_WRITE);

  // if (!file) {
  //   Serial.println("Unable to retrieve credentials file.");
  //   return;
  // }

  // std::string* v[5] = {
  //   &m_wifiHost,
  //   &m_wifiSsid,
  //   &m_wifiPass,
  //   &m_apSsid,
  //   &m_apPass
  // };

  Serial.println("Persist loop");
  // for (int i = 0; i < 5; i++) {
  //   // data += *v[i];
  //   // data += '\n';
  //   data.append(*(v[i]));
  //   data.append("\n");
  // }
  std::string data = m_wifiHost + "\n" +
                     m_wifiSsid + "\n" +
                     m_wifiPass + "\n" +
                     m_apSsid + "\n" +
                     m_apPass + "\n";

  Serial.println("Before file.print()");
  Serial.print(data.c_str());
  Serial.println(m_apSsid.c_str());
  Serial.println(m_apPass.c_str());
  // file.print(data);

  // file.close();
}
