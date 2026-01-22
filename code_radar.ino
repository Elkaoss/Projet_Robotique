/*
 * ROBOT RADAR AUTONOME - VERSION PRO (Filtrée & State Machine)
 * Matériel : mBot (mCore), Capteur Ultrason (Port 3)
 */

#include <MeMCore.h>

// --- CONFIGURATION MATERIELLE ---
MeUltrasonic ultr(PORT_3);
MeDCMotor MotorL(M1);
MeDCMotor MotorR(M2);
MeRGBLed rgb(7, 2);
MeBuzzer buzzer;

// --- REGLAGES FINS (A calibrer !) ---
const int SPEED_FWD = 160;       // Vitesse avant
const int SPEED_SCAN = 145;      // Vitesse très lente pour le scan (précision)
const int SPEED_TURN = 160;      // Vitesse pour le virage 90°
const int MOTOR_KICK = 200;      // Impulsion de départ pour vaincre l'inertie

// Temps (ms)
const unsigned long DUREE_SCAN_360 = 2600; // Temps pour faire un tour complet
const unsigned long DUREE_TURN_90  = 550;  // Temps pour faire 90°
const unsigned long DELAI_SCAN_AUTO = 10000; // Scan auto toutes les 10s

// Distances (cm)
const int SEUIL_OBSTACLE = 45;
const int DIST_MAX = 400;

// --- ETATS DU ROBOT (Machine à états) ---
enum RobotState {
  IDLE,       // Au repos / Initialisation
  MOVING,     // Avance
  SCANNING,   // Tourne et cartographie
  AVOIDING    // Manœuvre d'évitement
};

RobotState currentState = IDLE;

// --- VARIABLES GLOBALES ---
unsigned long stateStartTime = 0;
unsigned long lastScanTime = 0;
int bestDirection = 0; // 1 = Droite, -1 = Gauche
long sumRight = 0;
long sumLeft = 0;

void setup() {
  Serial.begin(9600); // Débit rapide pour la télémétrie
  rgb.setNumber(16);
  
  // Séquence de démarrage
  rgb.setColor(0, 0, 50); // Bleu
  rgb.show();
  buzzer.tone(500, 100);
  delay(100);
  buzzer.tone(1000, 200);
  
  delay(1000); 
  changeState(MOVING);
}

void loop() {
  // Gestion principale selon l'état du robot
  switch (currentState) {
    
    case MOVING:
      manageMoving();
      break;

    case SCANNING:
      manageScanning();
      break;

    case AVOIDING:
      // L'évitement est bloquant (court), géré directement après le scan
      break;
  }
}

// --- GESTIONNAIRES D'ETATS ---

void manageMoving() {
  // 1. Lire la distance (filtrée pour éviter les faux positifs)
  float dist = getFilteredDistance();

  // 2. Vérifier Obstacle
  if (dist > 0 && dist < SEUIL_OBSTACLE) {
    Stop();
    buzzer.tone(1500, 100); // Alerte sonore
    Serial.println("EVENT:OBSTACLE_DETECTED");
    changeState(SCANNING);
    return;
  }

  // 3. Vérifier Temps (Scan périodique)
  if (millis() - lastScanTime > DELAI_SCAN_AUTO) {
    Stop();
    Serial.println("EVENT:AUTO_SCAN");
    changeState(SCANNING);
    return;
  }

  // 4. Sinon, on avance
  Forward();
}

void manageScanning() {
  unsigned long timeInScan = millis() - stateStartTime;
  
  // Fin du scan ?
  if (timeInScan >= DUREE_SCAN_360) {
    Stop();
    Serial.println("EVENT:SCAN_COMPLETE");
    
    // Décision intelligente basculée sur les sommes accumulées
    if (sumRight > sumLeft) bestDirection = 1; // Droite plus vide
    else bestDirection = -1; // Gauche plus vide

    performAvoidance(bestDirection); // Faire la manœuvre
    
    lastScanTime = millis(); // Reset chrono
    changeState(MOVING);     // Repartir
    return;
  }

  // --- LOGIQUE DE SCAN ---
  
  // Calcul de l'angle courant (estimation linéaire)
  float progress = (float)timeInScan / DUREE_SCAN_360;
  int currentAngle = (int)(progress * 360);
  
  // Lecture capteur
  float dist = ultr.distanceCm();
  if (dist == 0 || dist > DIST_MAX) dist = DIST_MAX; // Nettoyage

  // Envoi données
  Serial.print("A:"); Serial.print(currentAngle);
  Serial.print(":D:"); Serial.println(dist);

  // Accumulation pour l'intelligence (Droite = 0-180, Gauche = 180-360)
  // Note: Selon le sens de rotation, ajuster ces plages.
  if (currentAngle < 180) sumRight += (long)dist;
  else sumLeft += (long)dist;

  // Petit délai pour ne pas saturer le port série
  delay(30); 
}

// --- FONCTIONS AUXILIAIRES ---

void performAvoidance(int dir) {
  // Etat temporaire
  rgb.setColor(50, 50, 0); // Jaune
  rgb.show();
  
  // 1. Recul de sécurité
  Backward();
  delay(400);
  Stop();
  delay(200);

  // 2. Rotation vers la zone libre
  if (dir == 1) {
    Serial.println("MSG:DECISION_RIGHT");
    TurnRight();
  } else {
    Serial.println("MSG:DECISION_LEFT");
    TurnLeft();
  }
  
  delay(DUREE_TURN_90);
  Stop();
  delay(300);
  
  rgb.setColor(0, 50, 0); // Retour vert
  rgb.show();
}

void changeState(RobotState newState) {
  Stop(); // Toujours arrêter les moteurs entre les états
  currentState = newState;
  stateStartTime = millis();
  
  if (newState == SCANNING) {
    rgb.setColor(50, 0, 50); // Violet
    rgb.show();
    
    // Reset des compteurs d'intelligence
    sumRight = 0;
    sumLeft = 0;
    
    // KICK MOTEUR : Impulsion courte pour lancer la rotation lente
    MotorL.run(-MOTOR_KICK);
    MotorR.run(MOTOR_KICK);
    delay(50); 
    // Puis vitesse de croisière lente
    MotorL.run(-SPEED_SCAN);
    MotorR.run(SPEED_SCAN);
  }
  else if (newState == MOVING) {
    rgb.setColor(0, 50, 0); // Vert
    rgb.show();
  }
}

// --- FILTRE MEDIAN (Anti-Bruit) ---
float getFilteredDistance() {
  float d1 = ultr.distanceCm();
  delay(5); // Petit délai entre mesures
  float d2 = ultr.distanceCm();
  delay(5);
  float d3 = ultr.distanceCm();

  // Si une mesure est 0 (erreur), on la met au max pour ne pas fausser le tri
  if(d1 == 0) d1 = DIST_MAX;
  if(d2 == 0) d2 = DIST_MAX;
  if(d3 == 0) d3 = DIST_MAX;

  // Tri simple pour trouver la médiane (valeur du milieu)
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