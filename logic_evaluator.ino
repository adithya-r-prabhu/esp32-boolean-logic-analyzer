#include <WiFi.h>
#include <WebServer.h>
#include <TFT_eSPI.h>
#include <SPI.h>

const char* AP_SSID = "LogicEvaluator";
const char* AP_PASSWORD = "logic123";


const int RED_LED = 25;   
const int GREEN_LED = 26; 

TFT_eSPI tft = TFT_eSPI();

WebServer server(80);

const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;


struct LogicState {
  String expression;
  String variables;
  bool result;
  bool hasResult;
  String truthTable;
  int currentTruthRow;
  bool showingTruthTable;
} currentState = {"", "", false, false, "", -1, false};


struct LogicVariable {
  char name;
  bool value;
};

LogicVariable vars[8];
int varCount = 0;


struct TruthTableRow {
  String inputs;
  bool output;
};

TruthTableRow truthRows[256];
int truthRowCount = 0;
String truthVariables[8];
int truthVarCount = 0;

void setup() {
  Serial.begin(115200);
  

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  

  startupSequence();
  

  Serial.println("Starting WiFi Access Point...");
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  IPAddress ip = WiFi.softAPIP();
  Serial.println("WiFi AP started");
  Serial.print("Network: ");
  Serial.println(AP_SSID);
  Serial.print("IP: ");
  Serial.println(ip);
  
  displayNetworkInfo(ip);
  
  server.on("/", handleHomePage);
  server.on("/evaluate", HTTP_POST, handleLogicEvaluation);
  server.on("/status", handleStatus);
  server.on("/truthtable", HTTP_POST, handleTruthTable);
  server.on("/testrow", HTTP_POST, handleTestRow);
  
  server.begin();
  Serial.println("Web server started");
  
  displayReady();
}

void loop() {
  server.handleClient();
}

void startupSequence() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("ESP32 Logic Evaluator", 10, 20);
  tft.setTextSize(1);
  tft.drawString("Starting up...", 10, 60);

  for (int i = 0; i < 3; i++) {
    digitalWrite(RED_LED, HIGH);
    delay(200);
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
    delay(200);
    digitalWrite(GREEN_LED, LOW);
  }
  
  delay(1000);
}

void displayNetworkInfo(IPAddress ip) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN);
  tft.drawString("WiFi Ready", 10, 20);
  
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Network: " + String(AP_SSID), 10, 60);
  tft.drawString("Password: " + String(AP_PASSWORD), 10, 80);
  tft.drawString("IP: " + ip.toString(), 10, 100);
  tft.drawString("Open browser to access", 10, 120);
  
  delay(3000);
}

void displayReady() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN);
  tft.drawString("Ready", 10, 20);
  
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Waiting for expression...", 10, 60);
  tft.drawString("Use web interface to", 10, 80);
  tft.drawString("evaluate logic", 10, 100);
  
  drawLEDStatus();
}

void displayResult(String expression, String variables, bool result) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(result ? TFT_GREEN : TFT_RED);
  tft.drawString(result ? "TRUE" : "FALSE", 10, 20);
  
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Expression:", 10, 60);
  tft.drawString(expression.substring(0, 35), 10, 80);
  if (expression.length() > 35) {
    tft.drawString(expression.substring(35), 10, 100);
  }
  
  tft.drawString("Variables:", 10, 120);
  tft.drawString(variables, 10, 140);
  
  drawLEDStatus();
}

void displayTruthTable() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN);
  tft.drawString("Truth Table Generated", 10, 10);
  
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Expression: " + currentState.expression, 10, 30);
  
  String header = "";
  for (int i = 0; i < truthVarCount; i++) {
    header += truthVariables[i] + " ";
  }
  header += "| Out";
  tft.drawString(header, 10, 60);

  int maxRows = min(truthRowCount, 10);
  for (int i = 0; i < maxRows; i++) {
    String row = "";
    String inputs = truthRows[i].inputs;
  
    int pos = 0;
    while (pos < inputs.length()) {
      int equals = inputs.indexOf('=', pos);
      if (equals == -1) break;
      
      int comma = inputs.indexOf(',', equals);
      if (comma == -1) comma = inputs.length();
      
      char value = inputs.charAt(equals + 1);
      row += String(value) + " ";
      
      pos = comma + 1;
    }
    
    row += "| " + String(truthRows[i].output ? "1" : "0");
    
    if (i == currentState.currentTruthRow) {
      tft.setTextColor(TFT_YELLOW);
    } else {
      tft.setTextColor(TFT_WHITE);
    }
    
    tft.drawString(row, 10, 80 + i * 15);
  }
  
  if (truthRowCount > 10) {
    tft.setTextColor(TFT_GRAY);
    tft.drawString("... " + String(truthRowCount - 10) + " more rows", 10, 80 + 10 * 15);
  }
  
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Use web interface to", 10, 200);
  tft.drawString("test individual rows", 10, 215);
  
  drawLEDStatus();
}

void drawLEDStatus() {
  tft.fillRect(250, 200, 60, 30, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("LEDs:", 250, 205);
  
  tft.fillCircle(270, 220, 5, digitalRead(RED_LED) ? TFT_RED : TFT_DARKGREY);
  
  tft.fillCircle(290, 220, 5, digitalRead(GREEN_LED) ? TFT_GREEN : TFT_DARKGREY);
}

void updateLEDs(bool result) {
  if (result) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
  } else {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
  }
}

void handleHomePage() {
  server.send(200, "text/html", generateWebPage());
}

void handleLogicEvaluation() {
  if (!server.hasArg("expression") || !server.hasArg("variables")) {
    server.send(400, "application/json", "{\"error\":\"Missing parameters\"}");
    return;
  }
  
  currentState.expression = server.arg("expression");
  currentState.variables = server.arg("variables");
  currentState.showingTruthTable = false;

  if (parseVariables(currentState.variables)) {
    currentState.result = evaluateLogic(currentState.expression);
    currentState.hasResult = true;
    

    updateLEDs(currentState.result);
    

    displayResult(currentState.expression, currentState.variables, currentState.result);
    

    String response = "{";
    response += "\"result\":" + String(currentState.result ? "true" : "false") + ",";
    response += "\"expression\":\"" + currentState.expression + "\",";
    response += "\"variables\":\"" + currentState.variables + "\",";
    response += "\"led\":\"" + String(currentState.result ? "GREEN" : "RED") + "\"";
    response += "}";
    
    server.send(200, "application/json", response);
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid variable format\"}");
  }
}

void handleTruthTable() {
  if (!server.hasArg("expression")) {
    server.send(400, "application/json", "{\"error\":\"Missing expression\"}");
    return;
  }
  
  String expression = server.arg("expression");
  currentState.expression = expression;
  currentState.showingTruthTable = true;
  currentState.currentTruthRow = -1;
  
  String truthTableJson = generateTruthTable(expression);
  

  displayTruthTable();
  
  server.send(200, "application/json", truthTableJson);
}

void handleTestRow() {
  if (!server.hasArg("variables")) {
    server.send(400, "application/json", "{\"error\":\"Missing variables\"}");
    return;
  }
  
  String variables = server.arg("variables");
  

  for (int i = 0; i < truthRowCount; i++) {
    if (truthRows[i].inputs == variables) {
      currentState.currentTruthRow = i;
      break;
    }
  }
  
  if (parseVariables(variables)) {
    bool result = evaluateLogic(currentState.expression);
    

    updateLEDs(result);
    

    if (currentState.showingTruthTable) {
      displayTruthTable();
    } else {
      displayResult(currentState.expression, variables, result);
    }
    
    String response = "{";
    response += "\"result\":" + String(result ? "true" : "false") + ",";
    response += "\"led\":\"" + String(result ? "GREEN" : "RED") + "\"";
    response += "}";
    
    server.send(200, "application/json", response);
  } else {
    server.send(400, "application/json", "{\"error\":\"Invalid variable format\"}");
  }
}

void handleStatus() {
  String response = "{";
  response += "\"result\":" + String(currentState.result ? "true" : "false") + ",";
  response += "\"expression\":\"" + currentState.expression + "\",";
  response += "\"variables\":\"" + currentState.variables + "\",";
  response += "\"led\":\"" + String(currentState.result ? "GREEN" : "RED") + "\"";
  response += "}";
  
  server.send(200, "application/json", response);
}

String generateTruthTable(String expression) {
  String uniqueVars = "";
  for (int i = 0; i < expression.length(); i++) {
    char c = expression.charAt(i);
    if (isalpha(c) && uniqueVars.indexOf(c) == -1) {
      uniqueVars += c;
    }
  }
  
  truthVarCount = uniqueVars.length();
  for (int i = 0; i < truthVarCount; i++) {
    truthVariables[i] = String(uniqueVars.charAt(i));
  }
  
  int numRows = 1 << truthVarCount;
  truthRowCount = numRows;
  
  String json = "{\"variables\":[";
  for (int i = 0; i < truthVarCount; i++) {
    json += "\"" + String(uniqueVars.charAt(i)) + "\"";
    if (i < truthVarCount - 1) json += ",";
  }
  json += "],\"rows\":[";
  

  for (int row = 0; row < numRows; row++) {
    String varString = "";
    varCount = 0;
    for (int var = 0; var < truthVarCount; var++) {
      bool value = (row >> (truthVarCount - 1 - var)) & 1;
      vars[varCount].name = uniqueVars.charAt(var);
      vars[varCount].value = value;
      varCount++;
      
      if (varString.length() > 0) varString += ",";
      varString += String(uniqueVars.charAt(var)) + "=" + String(value ? "1" : "0");
    }
    

    bool result = evaluateLogic(expression);
    
    truthRows[row].inputs = varString;
    truthRows[row].output = result;
    
    json += "{\"inputs\":\"" + varString + "\",\"output\":" + String(result ? "true" : "false") + "}";
    if (row < numRows - 1) json += ",";
  }
  
  json += "]}";
  return json;
}

bool parseVariables(String input) {
  varCount = 0;
  input.trim();
  
  int pos = 0;
  while (pos < input.length() && varCount < 8) {
    int equals = input.indexOf('=', pos);
    if (equals == -1) break;
    
    int comma = input.indexOf(',', equals);
    if (comma == -1) comma = input.length();
    
    char varName = input.charAt(equals - 1);
    char varValue = input.charAt(equals + 1);
    
    if (varValue == '1' || varValue == '0') {
      vars[varCount].name = varName;
      vars[varCount].value = (varValue == '1');
      varCount++;
    }
    
    pos = comma + 1;
  }
  
  return varCount > 0;
}

bool getVariableValue(char name) {
  for (int i = 0; i < varCount; i++) {
    if (vars[i].name == name) {
      return vars[i].value;
    }
  }
  return false;
}

bool evaluateLogic(String expr) {
  expr.replace(" ", "");
  int pos = 0;
  return evaluateOr(expr, pos);
}

// Operator precedence implementation
// Level 1: OR, NOR (lowest precedence)
bool evaluateOr(String expr, int& pos) {
  bool result = evaluateXor(expr, pos);
  
  while (pos < expr.length()) {
    if (expr.charAt(pos) == '|') {
      pos++; // Skip '|'
      result = result || evaluateXor(expr, pos);
    } else if (pos < expr.length() - 1 && expr.charAt(pos) == '~' && expr.charAt(pos + 1) == '|') {
      pos += 2; // Skip '~|' (NOR)
      result = !(result || evaluateXor(expr, pos));
    } else {
      break;
    }
  }
  
  return result;
}

// Level 2: XOR, XNOR
bool evaluateXor(String expr, int& pos) {
  bool result = evaluateAnd(expr, pos);
  
  while (pos < expr.length()) {
    if (expr.charAt(pos) == '^') {
      pos++; // Skip '^'
      result = result ^ evaluateAnd(expr, pos);
    } else if (pos < expr.length() - 1 && expr.charAt(pos) == '~' && expr.charAt(pos + 1) == '^') {
      pos += 2; // Skip '~^' (XNOR)
      result = !(result ^ evaluateAnd(expr, pos));
    } else {
      break;
    }
  }
  
  return result;
}

// Level 3: AND, NAND (higher precedence)
bool evaluateAnd(String expr, int& pos) {
  bool result = evaluateFactor(expr, pos);
  
  while (pos < expr.length()) {
    if (expr.charAt(pos) == '&') {
      pos++; // Skip '&'
      result = result && evaluateFactor(expr, pos);
    } else if (pos < expr.length() - 1 && expr.charAt(pos) == '~' && expr.charAt(pos + 1) == '&') {
      pos += 2; // Skip '~&' (NAND)
      result = !(result && evaluateFactor(expr, pos));
    } else {
      break;
    }
  }
  
  return result;
}

// Level 4: NOT, parentheses, variables (highest precedence)
bool evaluateFactor(String expr, int& pos) {
  if (pos >= expr.length()) return false;
  
  char c = expr.charAt(pos);
  
  if (c == '!') {
    pos++; // Skip '!'
    return !evaluateFactor(expr, pos);
  } else if (c == '(') {
    pos++; // Skip '('
    bool result = evaluateOr(expr, pos);
    if (pos < expr.length() && expr.charAt(pos) == ')') {
      pos++; // Skip ')'
    }
    return result;
  } else if (isalpha(c)) {
    pos++; // Skip variable
    return getVariableValue(c);
  }
  
  return false;
}

String generateWebPage() {
  String html = "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
  html += "<title>ESP32 Logic Evaluator</title>";
  html += "<style>";
  html += "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 0; padding: 20px; background: #f5f5f7; color: #1d1d1f; }";
  html += ".container { max-width: 800px; margin: 0 auto; background: white; border-radius: 12px; padding: 32px; box-shadow: 0 4px 16px rgba(0, 0, 0, 0.1); }";
  html += "h1 { text-align: center; margin-bottom: 8px; font-size: 32px; font-weight: 600; }";
  html += ".subtitle { text-align: center; color: #86868b; margin-bottom: 32px; font-size: 17px; }";
  html += ".form-group { margin-bottom: 24px; }";
  html += "label { display: block; margin-bottom: 8px; font-weight: 500; font-size: 17px; }";
  html += "input[type=\"text\"] { width: 100%; padding: 12px 16px; border: 1px solid #d2d2d7; border-radius: 8px; font-size: 16px; background: #fafafa; box-sizing: border-box; }";
  html += "input[type=\"text\"]:focus { outline: none; border-color: #007aff; background: white; }";
  html += ".examples { background: #f2f2f7; padding: 16px; border-radius: 8px; margin-top: 12px; }";
  html += ".examples h4 { margin: 0 0 8px 0; font-size: 14px; font-weight: 600; color: #1d1d1f; }";
  html += ".examples code { display: block; font-family: 'SF Mono', Monaco, monospace; font-size: 13px; color: #86868b; margin: 4px 0; }";
  html += ".button-group { display: flex; gap: 12px; margin-bottom: 24px; }";
  html += ".evaluate-btn, .truthtable-btn { flex: 1; padding: 16px; border: none; border-radius: 8px; font-size: 17px; font-weight: 500; cursor: pointer; transition: background 0.2s; }";
  html += ".evaluate-btn { background: #007aff; color: white; }";
  html += ".evaluate-btn:hover { background: #0056b3; }";
  html += ".truthtable-btn { background: #34c759; color: white; }";
  html += ".truthtable-btn:hover { background: #248a3d; }";
  html += ".result { margin-top: 24px; padding: 20px; border-radius: 8px; text-align: center; }";
  html += ".result-true { background: #d4edda; color: #155724; border: 1px solid #c3e6cb; }";
  html += ".result-false { background: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }";
  html += ".result-waiting { background: #f8f9fa; color: #6c757d; border: 1px solid #e9ecef; }";
  html += ".result-text { font-size: 24px; font-weight: 600; margin-bottom: 8px; }";
  html += ".led-status { margin-top: 12px; display: flex; align-items: center; justify-content: center; gap: 8px; }";
  html += ".led-indicator { width: 12px; height: 12px; border-radius: 50%; background: #ccc; }";
  html += ".led-red { background: #dc3545; box-shadow: 0 0 8px rgba(220, 53, 69, 0.5); }";
  html += ".led-green { background: #28a745; box-shadow: 0 0 8px rgba(40, 167, 69, 0.5); }";
  html += ".truth-table { margin-top: 24px; background: white; border-radius: 8px; overflow: hidden; box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1); }";
  html += ".truth-table table { width: 100%; border-collapse: collapse; }";
  html += ".truth-table th, .truth-table td { padding: 12px; text-align: center; border-bottom: 1px solid #e9ecef; }";
  html += ".truth-table th { background: #f8f9fa; font-weight: 600; color: #495057; }";
  html += ".truth-table tr:hover { background: #f8f9fa; cursor: pointer; }";
  html += ".truth-table tr.selected { background: #007aff; color: white; }";
  html += ".truth-table-title { padding: 16px; background: #f8f9fa; font-weight: 600; border-bottom: 1px solid #e9ecef; }";
  html += ".precedence-info { background: #e3f2fd; padding: 16px; border-radius: 8px; margin-top: 12px; }";
  html += ".precedence-info h4 { margin: 0 0 8px 0; color: #1565c0; }";
  html += ".precedence-info code { display: block; font-family: 'SF Mono', Monaco, monospace; font-size: 13px; color: #1565c0; margin: 4px 0; }";
  html += ".hidden { display: none; }";
  html += ".hardware-status { background: #e8f5e8; padding: 16px; border-radius: 8px; margin-top: 12px; text-align: center; }";
  html += ".hardware-status h4 { margin: 0 0 8px 0; color: #2e7d32; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class=\"container\">";
  html += "<h1>ESP32 Logic Evaluator</h1>";
  html += "<p class=\"subtitle\">Hardware-integrated Boolean logic evaluation with display and LEDs</p>";
  
  html += "<div class=\"hardware-status\">";
  html += "<h4>Hardware Status</h4>";
  html += "<div>Display: 2.4\" TFT Active</div>";
  html += "<div>LEDs: Red (FALSE) | Green (TRUE)</div>";
  html += "</div>";
  
  html += "<form id=\"logicForm\">";
  html += "<div class=\"form-group\">";
  html += "<label for=\"expression\">Expression</label>";
  html += "<input type=\"text\" id=\"expression\" name=\"expression\" placeholder=\"!(A & B) | (C ^ D)\" required>";
  html += "<div class=\"examples\">";
  html += "<h4>Available Operators</h4>";
  html += "<code>& (AND), | (OR), ! (NOT), ^ (XOR)</code>";
  html += "<code>~& (NAND), ~| (NOR), ~^ (XNOR)</code>";
  html += "<code>() (grouping)</code>";
  html += "</div>";
  html += "<div class=\"precedence-info\">";
  html += "<h4>Operator Precedence (High to Low)</h4>";
  html += "<code>1. () parentheses, ! NOT</code>";
  html += "<code>2. & AND, ~& NAND, ^ XOR, ~^ XNOR</code>";
  html += "<code>3. | OR, ~| NOR</code>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class=\"form-group\">";
  html += "<label for=\"variables\">Variables</label>";
  html += "<input type=\"text\" id=\"variables\" name=\"variables\" placeholder=\"A=1,B=0,C=1,D=0\" required>";
  html += "<div class=\"examples\">";
  html += "<h4>Format</h4>";
  html += "<code>A=1,B=0,C=1,D=0 (1=true, 0=false)</code>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class=\"button-group\">";
  html += "<button type=\"submit\" class=\"evaluate-btn\">Evaluate</button>";
  html += "<button type=\"button\" class=\"truthtable-btn\" onclick=\"generateTruthTable()\">Truth Table</button>";
  html += "</div>";
  html += "</form>";
  
  html += "<div class=\"result result-waiting\" id=\"result\">";
  html += "<div class=\"result-text\">Ready</div>";
  html += "<div>Enter an expression to evaluate</div>";
  html += "<div class=\"led-status\">";
  html += "Hardware LEDs: <div class=\"led-indicator\" id=\"ledIndicator\"></div> <span id=\"ledStatus\">OFF</span>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class=\"truth-table hidden\" id=\"truthTable\">";
  html += "<div class=\"truth-table-title\">Truth Table - Click any row to test on hardware</div>";
  html += "<table>";
  html += "<thead id=\"truthTableHead\"></thead>";
  html += "<tbody id=\"truthTableBody\"></tbody>";
  html += "</table>";
  html += "</div>";
  html += "</div>";
  

  html += "<script>";
  html += "const form = document.getElementById('logicForm');";
  html += "const result = document.getElementById('result');";
  html += "const ledIndicator = document.getElementById('ledIndicator');";
  html += "const ledStatus = document.getElementById('ledStatus');";
  html += "const truthTable = document.getElementById('truthTable');";
  html += "const truthTableHead = document.getElementById('truthTableHead');";
  html += "const truthTableBody = document.getElementById('truthTableBody');";
  
  html += "form.addEventListener('submit', async (e) => {";
  html += "e.preventDefault();";
  html += "const expression = document.getElementById('expression').value.trim();";
  html += "const variables = document.getElementById('variables').value.trim();";
  html += "if (!expression || !variables) { showError('Please fill in both fields'); return; }";
  html += "showLoading();";
  html += "try {";
  html += "const formData = new FormData();";
  html += "formData.append('expression', expression);";
  html += "formData.append('variables', variables);";
  html += "const response = await fetch('/evaluate', { method: 'POST', body: formData });";
  html += "const data = await response.json();";
  html += "if (response.ok) { showResult(data); } else { showError(data.error || 'Evaluation failed'); }";
  html += "} catch (err) { showError('Network error'); }";
  html += "});";
  
  html += "async function generateTruthTable() {";
  html += "const expression = document.getElementById('expression').value.trim();";
  html += "if (!expression) { showError('Please enter an expression first'); return; }";
  html += "showLoading();";
  html += "try {";
  html += "const formData = new FormData();";
  html += "formData.append('expression', expression);";
  html += "const response = await fetch('/truthtable', { method: 'POST', body: formData });";
  html += "const data = await response.json();";
  html += "if (response.ok) {";
  html += "displayTruthTable(data);";
  html += "result.className = 'result result-waiting';";
  html += "result.innerHTML = '<div class=\"result-text\">Truth Table Generated</div><div>Check display and click rows to test</div><div class=\"led-status\">Hardware LEDs: <div class=\"led-indicator\" id=\"ledIndicator\"></div> <span id=\"ledStatus\">OFF</span></div>';";
  html += "} else { showError(data.error || 'Truth table generation failed'); }";
  html += "} catch (err) { showError('Network error'); }";
  html += "}";
  
  html += "function displayTruthTable(data) {";
  html += "let headerHtml = '<tr>';";
  html += "data.variables.forEach(variable => { headerHtml += '<th>' + variable + '</th>'; });";
  html += "headerHtml += '<th>Output</th></tr>';";
  html += "truthTableHead.innerHTML = headerHtml;";
  html += "let bodyHtml = '';";
  html += "data.rows.forEach((row, index) => {";
  html += "bodyHtml += '<tr onclick=\"testTruthTableRow(\\'' + row.inputs + '\\')\" data-inputs=\"' + row.inputs + '\">';";
  html += "const inputs = row.inputs.split(',');";
  html += "inputs.forEach(input => {";
  html += "const value = input.split('=')[1];";
  html += "bodyHtml += '<td>' + value + '</td>';";
  html += "});";
  html += "bodyHtml += '<td>' + (row.output ? '1' : '0') + '</td>';";
  html += "bodyHtml += '</tr>';";
  html += "});";
  html += "truthTableBody.innerHTML = bodyHtml;";
  html += "truthTable.classList.remove('hidden');";
  html += "}";
  
  html += "async function testTruthTableRow(inputs) {";
  html += "document.querySelectorAll('.truth-table tr').forEach(tr => { tr.classList.remove('selected'); });";
  html += "event.target.closest('tr').classList.add('selected');";
  html += "try {";
  html += "const formData = new FormData();";
  html += "formData.append('variables', inputs);";
  html += "const response = await fetch('/testrow', { method: 'POST', body: formData });";
  html += "const data = await response.json();";
  html += "if (response.ok) {";
  html += "updateLED(data.result, data.led);";
  html += "result.className = 'result ' + (data.result ? 'result-true' : 'result-false');";
  html += "result.innerHTML = '<div class=\"result-text\">' + (data.result ? 'TRUE' : 'FALSE') + '</div><div>Testing: ' + inputs + '</div><div class=\"led-status\">Hardware LEDs: <div class=\"led-indicator ' + (data.result ? 'led-green' : 'led-red') + '\" id=\"ledIndicator\"></div> <span id=\"ledStatus\">' + data.led + '</span></div>';";
  html += "} else { showError(data.error || 'Test failed'); }";
  html += "} catch (err) { showError('Network error'); }";
  html += "}";
  
  html += "function showResult(data) {";
  html += "const isTrue = data.result;";
  html += "result.className = 'result ' + (isTrue ? 'result-true' : 'result-false');";
  html += "result.innerHTML = '<div class=\"result-text\">' + (isTrue ? 'TRUE' : 'FALSE') + '</div><div>' + data.expression + '</div><div class=\"led-status\">Hardware LEDs: <div class=\"led-indicator ' + (isTrue ? 'led-green' : 'led-red') + '\" id=\"ledIndicator\"></div> <span id=\"ledStatus\">' + data.led + '</span></div>';";
  html += "}";
  
  html += "function showLoading() {";
  html += "result.className = 'result result-waiting';";
  html += "result.innerHTML = '<div class=\"result-text\">Processing...</div><div>Updating hardware...</div><div class=\"led-status\">Hardware LEDs: <div class=\"led-indicator\"></div> <span>-</span></div>';";
  html += "}";
  
  html += "function showError(message) {";
  html += "result.className = 'result result-waiting';";
  html += "result.innerHTML = '<div class=\"result-text\">Error</div><div>' + message + '</div><div class=\"led-status\">Hardware LEDs: <div class=\"led-indicator\"></div> <span>OFF</span></div>';";
  html += "}";
  
  html += "function updateLED(isOn, status) {";
  html += "const indicator = document.getElementById('ledIndicator');";
  html += "const statusText = document.getElementById('ledStatus');";
  html += "indicator.className = 'led-indicator ' + (isOn ? 'led-green' : 'led-red');";
  html += "if (statusText) { statusText.textContent = status; }";
  html += "}";
  
  html += "</script>";
  html += "</body>";
  html += "</html>";
  
  return html;
}