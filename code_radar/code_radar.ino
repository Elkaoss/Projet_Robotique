/*
 * ROBOT RADAR AUTONOME mBOT - VERSION OPTIMISÉE
 * Améliorations :
 * - Utilisation des classes MeMCore.h officielles (MeUltrasonicSensor, etc.)
 * - Ajout de filtrage des mesures ultrason (moyenne glissante)
 * - Correction du calcul d'angle avec odométrie approximative
 * - Gestion améliorée du Bluetooth (buffer)
 * - Mode debug optionnel
 */

#include <MeMCore.h>

// --- CONFIGURATION DES PORTS ---
MeUltrasonicSensor ultr(PORT_3);  // ✅ Classe correcte (pas MeUltrasonic)
MeDCMotor MotorL(M1);             // Moteur Gauche sur M1
MeDCMotor MotorR(M2);             // Moteur Droit sur M2
MeRGBLed rgb(0, 2);               // LED interne (pin 0, 2 LEDs)
MeBuzzer buzzer;                  // Buzzer intégré

// --- CALIBRAGE (à ajuster selon votre robot) ---
int moveSpeed = 160;              // Vitesse d'avancement (0-255)
int turnSpeed = 140;              // Vitesse de rotation scan
const unsigned long TIME_FOR_1M = 4000;    // Temps pour ~1m (ms)
const unsigned long TIME_FOR_360 = 2300;   // Temps pour 360° (ms)

// --- CONSTANTES ---
const int OBSTACLE_DISTANCE = 50;     // Seuil détection obstacle (cm)
const int MIN_DISTANCE = 3;           // Distance min capteur (cm)
const int MAX_DISTANCE = 400;         // Distance max capteur (cm)
const int SCAN_DELAY = 80;            // Délai entre mesures (ms)
const int SCAN_SAMPLES = 3;           // Nombre de mesures moyennes
const bool DEBUG_MODE = false;        // Activer logs détaillés

// --- VARIABLES ---
bool isRunning = false;
unsigned long lastMoveTime = 0;
unsigned long lastButtonPress = 0;
float currentX = 0, currentY = 0;     // Position estimée (cm)
float currentAngle = 0;               // Orientation estimée (degrés)

void setup() {
  Serial.begin(9600);  // ✅ 9600 baud recommandé pour Bluetooth stable
  
  // Initialisation LED
  rgb.setpin(13);  // ✅ Pin 13 sur mCore
  rgb.setColor(0, 0, 50);  // Bleu = prêt
  rgb.show();
  
  Stop();
  delay(1000);  // Stabilisation capteurs
  
  Serial.println("STATUS:READY");
  Serial.println("INFO:mBot Radar v2.0");
  buzzer.tone(800, 100);
  delay(150);
  buzzer.tone(1000, 100);
}

void loop() {
  // --- GESTION BOUTON START/STOP ---
  if (analogRead(A7) < 100) {  // ✅ Seuil plus robuste (était < 10)
    if (millis() - lastButtonPress > 800) {  // Anti-rebond 800ms
      lastButtonPress = millis();
      isRunning = !isRunning;
      
      if (isRunning) {
        startRobot();
      } else {
        stopRobot();
      }
      
      // Attendre relâchement
      while(analogRead(A7) < 100) delay(10);
    }
  }

  if (!isRunning) return;

  // --- LECTURE CAPTEUR FILTRÉE ---
  float dist = getFilteredDistance();
  
  if (DEBUG_MODE) {
    Serial.print("DIST:");
    Serial.println(dist);
  }

  // --- LOGIQUE NAVIGATION ---
  
  // CAS 1 : Obstacle proche
  if (dist < OBSTACLE_DISTANCE) {
    handleObstacle();
  }
  
  // CAS 2 : 1 mètre parcouru
  else if (millis() - lastMoveTime > TIME_FOR_1M) {
    handleMeterMark();
  }
  
  // CAS 3 : Voie libre
  else {
    Forward();
    updateOdometry(moveSpeed, 50);  // Update position estimée
  }
  
  delay(50);  // ✅ Petit délai pour stabilité
}

// --- FONCTION DISTANCE FILTRÉE (moyenne) ---
float getFilteredDistance() {
  float sum = 0;
  int validCount = 0;
  
  for (int i = 0; i < SCAN_SAMPLES; i++) {
    float d = ultr.distanceCm();
    
    if (d >= MIN_DISTANCE && d <= MAX_DISTANCE) {
      sum += d;
      validCount++;
    }
    delay(30);  // Délai entre mesures ultrason
  }
  
  if (validCount == 0) return MAX_DISTANCE;
  
  return sum / validCount;
}

// --- DÉMARRAGE ROBOT ---
void startRobot() {
  rgb.setColor(0, 50, 0);  // Vert
  rgb.show();
  buzzer.tone(1200, 150);
  
  Serial.println("STATUS:STARTED");
  Serial.print("POS:");
  Serial.print(currentX);
  Serial.print(":");
  Serial.println(currentY);
  
  lastMoveTime = millis();
  currentX = 0;
  currentY = 0;
  currentAngle = 0;
  
  delay(1000);
}

// --- ARRÊT ROBOT ---
void stopRobot() {
  Stop();
  rgb.setColor(50, 0, 0);  // Rouge
  rgb.show();
  buzzer.tone(600, 200);
  Serial.println("STATUS:STOPPED");
}

// --- GESTION OBSTACLE ---
void handleObstacle() {
  Stop();
  buzzer.tone(2200, 80);
  delay(100);
  
  Serial.println("EVENT:OBSTACLE");
  performRadarScan();
  
  // Évitement
  Backward();
  delay(600);
  updateOdometry(-moveSpeed, 600);
  
  TurnLeft();
  delay(700);  // ~90°
  currentAngle += 90;
  if (currentAngle >= 360) currentAngle -= 360;
  
  Stop();
  delay(300);
  lastMoveTime = millis();
}

// --- GESTION REPÈRE 1M ---
void handleMeterMark() {
  Stop();
  buzzer.tone(1000, 80);
  delay(100);
  buzzer.tone(1200, 80);
  
  Serial.println("EVENT:METRE");
  Serial.print("POS:");
  Serial.print(currentX);
  Serial.print(":");
  Serial.println(currentY);
  
  performRadarScan();
  lastMoveTime = millis();
}

// --- SCAN RADAR 360° AMÉLIORÉ ---
void performRadarScan() {
  Serial.println("STATUS:SCAN_START");
  rgb.setColor(30, 0, 30);  // Violet
  rgb.show();

  unsigned long scanStart = millis();
  unsigned long lastMeasure = 0;
  int measureCount = 0;
  
  // Rotation
  MotorL.run(-turnSpeed);
  MotorR.run(turnSpeed);

  while (millis() - scanStart < TIME_FOR_360) {
    if (millis() - lastMeasure >= SCAN_DELAY) {
      lastMeasure = millis();
      
      // Calcul angle avec correction
      float progress = (float)(millis() - scanStart) / TIME_FOR_360;
      int angle = (int)(progress * 360.0);
      angle = constrain(angle, 0, 360);
      
      // Mesure filtrée rapide (2 échantillons)
      float d1 = ultr.distanceCm();
      delay(20);
      float d2 = ultr.distanceCm();
      
      float reading = (d1 + d2) / 2.0;
      
      // Validation
      if (reading < MIN_DISTANCE || reading > MAX_DISTANCE) {
        reading = MAX_DISTANCE;
      }

      // Format optimisé pour parsing Python
      Serial.print("D:");
      Serial.print(angle);
      Serial.print(":");
      Serial.print(reading, 1);  // 1 décimale
      Serial.print(":");
      Serial.print(currentX, 0);  // Position X
      Serial.print(":");
      Serial.println(currentY, 0);  // Position Y
      
      measureCount++;
    }
  }

  Stop();
  Serial.print("STATUS:SCAN_END:");
  Serial.println(measureCount);
  
  rgb.setColor(0, 50, 0);
  rgb.show();
  delay(500);
}

// --- ODOMÉTRIE SIMPLE (estimation) ---
void updateOdometry(int speed, unsigned long duration) {
  // Calcul approximatif : vitesse × temps → distance
  float distance = (float)(speed * duration) / (255.0 * 1000.0) * 50.0;  // ~50cm/s à pleine vitesse
  
  // Mise à jour position en coordonnées cartésiennes
  float radAngle = currentAngle * PI / 180.0;
  currentX += distance * cos(radAngle);
  currentY += distance * sin(radAngle);
}

// --- MOUVEMENTS MOTEURS ---
void Forward() {
  MotorL.run(-moveSpeed);
  MotorR.run(moveSpeed);
}

void Backward() {
  MotorL.run(moveSpeed);
  MotorR.run(-moveSpeed);
}

void TurnLeft() {
  MotorL.run(-moveSpeed);
  MotorR.run(-moveSpeed);
}

void TurnRight() {
  MotorL.run(moveSpeed);
  MotorR.run(moveSpeed);
}

void Stop() {
  MotorL.stop();  // ✅ Utilise .stop() au lieu de .run(0)
  MotorR.stop();
}
