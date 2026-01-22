/*
 * ROBOT RADAR AUTONOME - VERSION INTELLIGENTE
 * Le robot scanne 360° et se dirige vers la zone la plus dégagée
 */

#include <MeMCore.h>

// --- CONFIGURATION MATERIELLE ---
MeUltrasonicSensor ultr(PORT_3);
MeDCMotor MotorL(M1);
MeDCMotor MotorR(M2);
MeRGBLed rgb(7, 2);
MeBuzzer buzzer;

// --- REGLAGES FINS ---
const int SPEED_FWD = 160;
const int SPEED_SCAN = 145;
const int SPEED_TURN = 160;
const int MOTOR_KICK = 200;

// Temps (ms)
const unsigned long DUREE_SCAN_360 = 2600;
const unsigned long DUREE_TURN_90 = 550;
const unsigned long DELAI_SCAN_AUTO = 10000;

// Distances (cm)
const int SEUIL_OBSTACLE = 45;
const int DIST_MAX = 400;

// ✅ Configuration du scan sectorisé (8 directions)
const int NB_SECTEURS = 8;  // Diviser le cercle en 8 directions
float secteurDistances[8] = {0};  // Stocker la distance max de chaque secteur
int secteurCounts[8] = {0};       // Nombre de mesures par secteur

// --- ETATS DU ROBOT ---
enum RobotState {
  IDLE,
  MOVING,
  SCANNING,
  AVOIDING
};

RobotState currentState = IDLE;

// --- VARIABLES GLOBALES ---
unsigned long stateStartTime = 0;
unsigned long lastScanTime = 0;
int bestSector = 0;  // ✅ Secteur avec la plus grande distance
float maxDistance = 0;  // ✅ Distance maximale trouvée

void setup() {
  Serial.begin(9600);
  rgb.setNumber(16);
  
  // Séquence de démarrage
  rgb.setColor(0, 0, 50);
  rgb.show();
  buzzer.tone(500, 100);
  delay(100);
  buzzer.tone(1000, 200);
  
  delay(1000);
  changeState(MOVING);
}

void loop() {
  switch (currentState) {
    case MOVING:
      manageMoving();
      break;

    case SCANNING:
      manageScanning();
      break;

    case AVOIDING:
      break;
  }
}

// --- GESTIONNAIRES D'ETATS ---

void manageMoving() {
  float dist = getFilteredDistance();

  // Obstacle détecté
  if (dist > 0 && dist < SEUIL_OBSTACLE) {
    Stop();
    buzzer.tone(1500, 100);
    Serial.println("EVENT:OBSTACLE_DETECTED");
    changeState(SCANNING);
    return;
  }

  // Scan périodique
  if (millis() - lastScanTime > DELAI_SCAN_AUTO) {
    Stop();
    Serial.println("EVENT:AUTO_SCAN");
    changeState(SCANNING);
    return;
  }

  Forward();
}

void manageScanning() {
  unsigned long timeInScan = millis() - stateStartTime;
  
  // Fin du scan ?
  if (timeInScan >= DUREE_SCAN_360) {
    Stop();
    Serial.println("EVENT:SCAN_COMPLETE");
    
    // ✅ ANALYSE: Trouver le secteur avec la plus grande distance moyenne
    findBestDirection();
    
    // ✅ Afficher les résultats
    Serial.print("MSG:BEST_SECTOR:");
    Serial.print(bestSector);
    Serial.print(":DISTANCE:");
    Serial.println(maxDistance);
    
    // ✅ Tourner vers la meilleure direction
    performAvoidance(bestSector);
    
    lastScanTime = millis();
    changeState(MOVING);
    return;
  }

  // --- LOGIQUE DE SCAN ---
  
  // Calcul de l'angle courant
  float progress = (float)timeInScan / DUREE_SCAN_360;
  int currentAngle = (int)(progress * 360);
  
  // Lecture capteur
  float dist = ultr.distanceCm();
  if (dist == 0 || dist > DIST_MAX) dist = DIST_MAX;

  // Envoi données
  Serial.print("A:");
  Serial.print(currentAngle);
  Serial.print(":D:");
  Serial.println(dist);

  // ✅ ACCUMULATION PAR SECTEUR
  // Chaque secteur = 45° (360/8)
  int secteur = currentAngle / 45;
  if (secteur >= NB_SECTEURS) secteur = NB_SECTEURS - 1;
  
  // Garder la distance MAX du secteur (pas la moyenne)
  if (dist > secteurDistances[secteur]) {
    secteurDistances[secteur] = dist;
  }
  secteurCounts[secteur]++;

  delay(30);
}

// --- ✅ FONCTION D'ANALYSE INTELLIGENTE ---
void findBestDirection() {
  maxDistance = 0;
  bestSector = 0;
  
  Serial.println("MSG:ANALYSE_SECTEURS");
  
  // Trouver le secteur avec la plus grande distance
  for (int i = 0; i < NB_SECTEURS; i++) {
    // Afficher les résultats de chaque secteur
    Serial.print("SECTEUR:");
    Serial.print(i);
    Serial.print("(");
    Serial.print(i * 45);
    Serial.print("-");
    Serial.print((i + 1) * 45);
    Serial.print("°):DIST_MAX:");
    Serial.print(secteurDistances[i]);
    Serial.print(":MESURES:");
    Serial.println(secteurCounts[i]);
    
    // Trouver le maximum
    if (secteurDistances[i] > maxDistance) {
      maxDistance = secteurDistances[i];
      bestSector = i;
    }
  }
  
  Serial.print("MSG:MEILLEURE_DIRECTION:");
  Serial.print(bestSector * 45);
  Serial.print("-");
  Serial.print((bestSector + 1) * 45);
  Serial.print("° (DIST:");
  Serial.print(maxDistance);
  Serial.println("cm)");
}

// --- ✅ EVITEMENT VERS LA MEILLEURE DIRECTION ---
void performAvoidance(int sector) {
  rgb.setColor(50, 50, 0);
  rgb.show();
  
  // 1. Recul de sécurité
  Backward();
  delay(400);
  Stop();
  delay(200);

  // 2. ✅ Calculer l'angle à tourner
  // Secteur 0 = 0-45° (devant droite)
  // Secteur 1 = 45-90° (droite)
  // Secteur 2 = 90-135° (arrière droite)
  // Secteur 3 = 135-180° (arrière)
  // Secteur 4 = 180-225° (arrière gauche)
  // Secteur 5 = 225-270° (gauche)
  // Secteur 6 = 270-315° (avant gauche)
  // Secteur 7 = 315-360° (devant gauche)
  
  int angleCible = sector * 45 + 22;  // Milieu du secteur
  int angleTourner = angleCible;
  
  // Si l'angle est > 180°, tourner dans l'autre sens
  if (angleTourner > 180) {
    angleTourner = 360 - angleTourner;
    Serial.print("MSG:TURN_LEFT:");
    Serial.print(angleTourner);
    Serial.println("°");
    
    TurnLeft();
    // Temps proportionnel à l'angle (550ms pour 90°)
    delay((unsigned long)(DUREE_TURN_90 * angleTourner / 90.0));
  } else {
    Serial.print("MSG:TURN_RIGHT:");
    Serial.print(angleTourner);
    Serial.println("°");
    
    TurnRight();
    delay((unsigned long)(DUREE_TURN_90 * angleTourner / 90.0));
  }
  
  Stop();
  delay(300);
  
  buzzer.tone(1200, 100);  // Confirmation sonore
  
  rgb.setColor(0, 50, 0);
  rgb.show();
}

void changeState(RobotState newState) {
  Stop();
  currentState = newState;
  stateStartTime = millis();
  
  if (newState == SCANNING) {
    rgb.setColor(50, 0, 50);
    rgb.show();
    
    // ✅ Reset des données de scan
    for (int i = 0; i < NB_SECTEURS; i++) {
      secteurDistances[i] = 0;
      secteurCounts[i] = 0;
    }
    maxDistance = 0;
    bestSector = 0;
    
    // Impulsion de démarrage
    MotorL.run(-MOTOR_KICK);
    MotorR.run(-MOTOR_KICK);
    delay(50);
    MotorL.run(-SPEED_SCAN);
    MotorR.run(-SPEED_SCAN);
  }
  else if (newState == MOVING) {
    rgb.setColor(0, 50, 0);
    rgb.show();
  }
}

// --- FILTRE MEDIAN ---
float getFilteredDistance() {
  float d1 = ultr.distanceCm();
  delay(5);
  float d2 = ultr.distanceCm();
  delay(5);
  float d3 = ultr.distanceCm();

  if(d1 == 0) d1 = DIST_MAX;
  if(d2 == 0) d2 = DIST_MAX;
  if(d3 == 0) d3 = DIST_MAX;

  if ((d1 <= d2 && d2 <= d3) || (d3 <= d2 && d2 <= d1)) return d2;
  if ((d2 <= d1 && d1 <= d3) || (d3 <= d1 && d1 <= d2)) return d1;
  return d3;
}

// --- MOTEURS ---
void Forward() { MotorL.run(-SPEED_FWD); MotorR.run(SPEED_FWD); }
void Backward() { MotorL.run(SPEED_FWD); MotorR.run(-SPEED_FWD); }
void TurnLeft() { MotorL.run(-SPEED_TURN); MotorR.run(-SPEED_TURN); }
void TurnRight() { MotorL.run(SPEED_TURN); MotorR.run(SPEED_TURN); }
void Stop() { MotorL.run(0); MotorR.run(0); }
