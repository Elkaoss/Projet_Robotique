/*
 * ROBOT RADAR AUTONOME mBOT
 * Fonctionnalités :
 * 1. Navigation autonome (sans télécommande).
 * 2. Arrêt et Scan Radar si obstacle < 50cm.
 * 3. Arrêt et Scan Radar automatique tous les 1 mètre (estimé par temps).
 * 4. Envoi des données vers le PC pour cartographie Python.
 */

#include <MeMCore.h>
#include <Wire.h>
#include <SoftwareSerial.h>

// --- CONFIGURATION DES PORTS ---
MeUltrasonic ultr(PORT_3);   // Capteur Ultrason sur le Port 3
MeDCMotor MotorL(M1);        // Moteur Gauche sur M1
MeDCMotor MotorR(M2);        // Moteur Droit sur M2
MeRGBLed rgb(7, 2);          // LED intégrée
MeBuzzer buzzer;             // Buzzer intégré

// --- CALIBRAGE (A ajuster selon votre batterie et sol) ---
int moveSpeed = 160;         // Vitesse d'avancement
int turnSpeed = 140;         // Vitesse de rotation pour le scan
const unsigned long TIME_FOR_1M = 4000;  // Temps pour faire ~1 mètre (ms)
const unsigned long TIME_FOR_360 = 2300; // Temps pour faire un tour complet (ms)

// --- VARIABLES ---
bool isRunning = false;          // État du robot
unsigned long lastMoveTime = 0;  // Chronomètre pour la distance
float dist = 0;                  // Distance lue

void setup() {
  Serial.begin(115200); // Vitesse rapide pour le transfert de données
  rgb.setNumber(16);
  
  // Signal de prêt (LED Bleue)
  rgb.setColor(0, 0, 50);
  rgb.show();
  Stop();
}

void loop() {
  // --- GESTION DU BOUTON START/STOP ---
  if (analogRead(A7) < 10) { // Si bouton appuyé
    delay(50); // Anti-rebond
    if (analogRead(A7) < 10) {
      isRunning = !isRunning; // Inverser l'état
      
      if (isRunning) {
        // Démarrage
        rgb.setColor(0, 50, 0); // Vert
        rgb.show();
        buzzer.tone(1000, 200);
        lastMoveTime = millis(); // Reset du chrono
        delay(1000); // Temps pour retirer sa main
      } else {
        // Arrêt
        Stop();
        rgb.setColor(50, 0, 0); // Rouge
        rgb.show();
        buzzer.tone(500, 200);
        while(analogRead(A7) < 10); // Attendre relâchement
      }
    }
  }

  // Si le robot est en pause, on arrête la boucle ici
  if (!isRunning) return;

  // --- LECTURE CAPTEURS ---
  dist = ultr.distanceCm();
  if (dist == 0) dist = 400; // Correction bruit capteur

  // --- LOGIQUE DE NAVIGATION ---

  // CAS 1 : Obstacle détecté (< 50cm)
  if (dist < 50) {
    Stop();
    buzzer.tone(2000, 100); // Bip aigu
    delay(200);
    
    // Envoyer signal à Python : "J'ai trouvé un obstacle, je scanne"
    Serial.println("EVENT:OBSTACLE"); 
    performRadarScan(); // Faire le 360

    // Manœuvre d'évitement
    Backward(); delay(600);
    TurnLeft(); delay(700); // Tourner d'environ 90° (ajuster délai)
    Stop();     delay(300);
    
    lastMoveTime = millis(); // Reset du compteur 1m
  }
  
  // CAS 2 : 1 Mètre parcouru (Temps écoulé)
  else if (millis() - lastMoveTime > TIME_FOR_1M) {
    Stop();
    buzzer.tone(1000, 100); delay(100); buzzer.tone(1000, 100);
    
    // Envoyer signal à Python : "J'ai fait 1m, je scanne"
    Serial.println("EVENT:METRE");
    performRadarScan(); // Faire le 360
    
    lastMoveTime = millis(); // Reset du compteur 1m
  }
  
  // CAS 3 : Voie libre -> Avancer
  else {
    Forward();
  }
}

// --- FONCTION DE SCAN RADAR ---
void performRadarScan() {
  Serial.println("STATUS:SCAN_START");
  rgb.setColor(30, 0, 30); // Violet
  rgb.show();

  unsigned long scanStart = millis();
  
  // Lancer la rotation
  MotorL.run(-turnSpeed);
  MotorR.run(turnSpeed);

  while (millis() - scanStart < TIME_FOR_360) {
    // Calcul de l'angle estimé (règle de trois sur le temps)
    float progress = (float)(millis() - scanStart) / TIME_FOR_360;
    int angle = progress * 360;
    
    // Lecture
    float reading = ultr.distanceCm();
    if(reading == 0) reading = 400; 

    // Envoi Format: "D:angle:distance"
    Serial.print("D:");
    Serial.print(angle);
    Serial.print(":");
    Serial.println(reading);
    
    delay(40); // Pause technique pour stabilité ultrason
  }

  Stop();
  Serial.println("STATUS:SCAN_END");
  rgb.setColor(0, 50, 0); // Retour vert
  rgb.show();
  delay(500);
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
void Stop() {
  MotorL.run(0);
  MotorR.run(0);
}