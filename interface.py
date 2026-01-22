import tkinter as tk
from tkinter import ttk
import math
import serial
import serial.tools.list_ports
import threading
import time


class RadarInterface:
    def __init__(self, root):
        self.root = root
        self.root.title("mBot Radar & Cartographie")
        self.root.geometry("1400x800")
        self.root.configure(bg='#000000')
        
        # Variables
        self.serial_port = None
        self.is_running = False
        self.current_angle = 0
        self.current_distance = 0
        self.radar_points = []  # Pour l'effet de traînée
        self.map_points = []  # Pour la cartographie persistante
        self.max_distance = 400  # Distance max capteur (cm)
        self.is_scanning = False
        self.scan_count = 0
        
        # Configuration interface
        self.setup_ui()
        
        # Démarrage animation
        self.animate_radar()
        
    def setup_ui(self):
        # Frame principal
        main_frame = tk.Frame(self.root, bg='#000000')
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Frame pour les contrôles en haut
        control_frame = tk.Frame(main_frame, bg='#000000')
        control_frame.pack(side=tk.TOP, fill=tk.X, pady=(0, 10))
        
        # Sélection du port série
        tk.Label(control_frame, text="Port Série:", bg='#000000', fg='#00ff00', 
                font=('Courier', 10)).pack(side=tk.LEFT, padx=5)
        
        self.port_var = tk.StringVar()
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo = ttk.Combobox(control_frame, textvariable=self.port_var, 
                                       values=ports, width=15, state='readonly')
        if ports:
            self.port_combo.current(0)
        self.port_combo.pack(side=tk.LEFT, padx=5)
        
        # Bouton refresh ports
        self.refresh_btn = tk.Button(control_frame, text="🔄", command=self.refresh_ports,
                                     bg='#001a00', fg='#00ff00', font=('Courier', 10),
                                     relief=tk.FLAT, padx=10)
        self.refresh_btn.pack(side=tk.LEFT, padx=5)
        
        # Bouton connexion
        self.connect_btn = tk.Button(control_frame, text="Connecter", 
                                     command=self.toggle_connection,
                                     bg='#003300', fg='#00ff00', font=('Courier', 10, 'bold'),
                                     relief=tk.FLAT, padx=15, pady=5)
        self.connect_btn.pack(side=tk.LEFT, padx=5)
        
        # Indicateur de statut
        self.status_label = tk.Label(control_frame, text="● Déconnecté", 
                                     bg='#000000', fg='#ff0000', font=('Courier', 10, 'bold'))
        self.status_label.pack(side=tk.LEFT, padx=20)
        
        # Indicateur de scan
        self.scan_label = tk.Label(control_frame, text="", 
                                   bg='#000000', fg='#ff00ff', font=('Courier', 10, 'bold'))
        self.scan_label.pack(side=tk.LEFT, padx=10)
        
        # Compteur de scans
        self.scan_counter = tk.Label(control_frame, text="Scans: 0", 
                                     bg='#000000', fg='#00ffff', font=('Courier', 10))
        self.scan_counter.pack(side=tk.LEFT, padx=10)
        
        # Bouton reset cartographie
        self.reset_btn = tk.Button(control_frame, text="Reset Carte", 
                                   command=self.reset_map,
                                   bg='#330000', fg='#ff0000', font=('Courier', 10),
                                   relief=tk.FLAT, padx=10)
        self.reset_btn.pack(side=tk.RIGHT, padx=5)
        
        # Frame pour les canvas
        canvas_frame = tk.Frame(main_frame, bg='#000000')
        canvas_frame.pack(fill=tk.BOTH, expand=True)
        
        # Frame Radar (gauche)
        radar_frame = tk.Frame(canvas_frame, bg='#000000', relief=tk.RAISED, bd=2)
        radar_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0, 5))
        
        tk.Label(radar_frame, text="RADAR SONAR", bg='#000000', fg='#00ff00',
                font=('Courier', 14, 'bold')).pack(pady=5)
        
        self.radar_canvas = tk.Canvas(radar_frame, bg='#000a00', 
                                     highlightthickness=2, highlightbackground='#00ff00')
        self.radar_canvas.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Info radar
        self.radar_info = tk.Label(radar_frame, text="Angle: 0° | Distance: 0 cm", 
                                  bg='#000000', fg='#00ff00', font=('Courier', 10))
        self.radar_info.pack(pady=5)
        
        # Frame Cartographie (droite)
        map_frame = tk.Frame(canvas_frame, bg='#000000', relief=tk.RAISED, bd=2)
        map_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(5, 0))
        
        tk.Label(map_frame, text="CARTOGRAPHIE 2D", bg='#000000', fg='#00ff00',
                font=('Courier', 14, 'bold')).pack(pady=5)
        
        self.map_canvas = tk.Canvas(map_frame, bg='#000a00',
                                   highlightthickness=2, highlightbackground='#00ff00')
        self.map_canvas.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Info cartographie
        self.map_info = tk.Label(map_frame, text="Points détectés: 0", 
                                bg='#000000', fg='#00ff00', font=('Courier', 10))
        self.map_info.pack(pady=5)
        
        # Log des événements
        log_frame = tk.Frame(main_frame, bg='#000000')
        log_frame.pack(side=tk.BOTTOM, fill=tk.X, pady=(10, 0))
        
        tk.Label(log_frame, text="LOG ÉVÉNEMENTS:", bg='#000000', fg='#00ff00',
                font=('Courier', 10, 'bold')).pack(anchor=tk.W)
        
        self.log_text = tk.Text(log_frame, height=4, bg='#001a00', fg='#00ff00',
                               font=('Courier', 8), relief=tk.FLAT)
        self.log_text.pack(fill=tk.X, padx=5, pady=5)
        
        # Dessiner les grilles initiales
        self.root.after(100, self.draw_radar_grid)
        self.root.after(100, self.draw_map_grid)
    
    def log_event(self, message):
        """Ajouter un message au log"""
        timestamp = time.strftime("%H:%M:%S")
        self.log_text.insert(tk.END, f"[{timestamp}] {message}\n")
        self.log_text.see(tk.END)
        lines = self.log_text.get("1.0", tk.END).split("\n")
        if len(lines) > 50:
            self.log_text.delete("1.0", "2.0")
    
    def refresh_ports(self):
        """Rafraîchir la liste des ports série"""
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports and not self.port_var.get():
            self.port_combo.current(0)
        self.log_event(f"Ports trouvés: {len(ports)}")
    
    def toggle_connection(self):
        """Connecter/déconnecter au port série"""
        if not self.is_running:
            try:
                port = self.port_var.get()
                if not port:
                    self.log_event("❌ Aucun port sélectionné")
                    return
                
                # ✅ Connexion robuste
                self.serial_port = serial.Serial(
                    port=port,
                    baudrate=9600,
                    timeout=0.5
                )
                time.sleep(2)  # Attendre stabilisation
                self.serial_port.reset_input_buffer()
                
                self.is_running = True
                self.connect_btn.config(text="Déconnecter", bg='#330000', fg='#ff0000')
                self.status_label.config(text="● Connecté", fg='#00ff00')
                self.log_event(f"✅ Connecté au port {port}")
                
                # Démarrer le thread de lecture
                self.read_thread = threading.Thread(target=self.read_serial, daemon=True)
                self.read_thread.start()
            except Exception as e:
                self.status_label.config(text=f"● Erreur", fg='#ff0000')
                self.log_event(f"❌ Erreur: {str(e)}")
        else:
            self.is_running = False
            if self.serial_port:
                self.serial_port.close()
            self.connect_btn.config(text="Connecter", bg='#003300', fg='#00ff00')
            self.status_label.config(text="● Déconnecté", fg='#ff0000')
            self.log_event("⏸️ Déconnecté")
    
    def read_serial(self):
    
    # Attendre stabilisation
        time.sleep(2)
        
        if self.serial_port:
            self.serial_port.reset_input_buffer()
        
        while self.is_running:
            try:
                if self.serial_port and self.serial_port.in_waiting > 0:
                    raw_data = self.serial_port.readline()
                    line = raw_data.decode('utf-8', errors='ignore').strip()
                    
                    if not line or len(line) < 3:
                        continue
                    
                    # ✅ FORMAT CORRIGÉ: "A:54:D:12.76"
                    if line.startswith("A:") and ":D:" in line:
                        try:
                            # Séparer A:angle:D:distance
                            parts = line.split(':')
                            if len(parts) >= 4:
                                angle = int(float(parts[1]))  # Après A:
                                distance = float(parts[3])     # Après D:
                                
                                # Mise à jour
                                self.current_angle = angle
                                self.current_distance = distance
                                
                                # Ajouter point
                                if angle <=7 :
                                    self.map_points = []
                                    
                                if 0 < distance < self.max_distance:
                                    
                                    self.add_map_point(angle, distance)
                                    
                                
                                print(f"Angle: {angle}° Distance: {distance}cm")  # Debug
                            
                                    
                        except (ValueError, IndexError) as e:
                            print(f"Erreur parsing: {e} - Ligne: {line}")
                    
                    # Événements
                    elif line.startswith("EVENT:"):
                        event = line.split(':', 1)[1]
                        if event == "OBSTACLE":
                            self.map_points = []
                            self.reset_map(self)
                            self.log_event("⚠️  Obstacle détecté")
                        elif event == "METRE":
                            self.map_points = []
                            self.reset_map(self)
                            self.log_event("📏 1 mètre parcouru")
                        elif event == "AUTO_SCAN":
                            self.map_points = []
                            self.reset_map(self)
                            self.log_event("📏 1 mètre parcouru")
                    
                    # Statuts
                    elif line.startswith("STATUS:"):
                        status_parts = line.split(':', 1)
                        if len(status_parts) > 1:
                            status = status_parts[1]
                            
                            if status == "SCAN_START":
                                self.reset_map(self)
                                self.map_points = []
                                self.is_scanning = True
                                self.scan_count += 1
                                self.scan_label.config(text="🔄 SCAN EN COURS")
                                self.scan_counter.config(text=f"Scans: {self.scan_count}")
                                self.log_event("🔄 Début du scan 360°")
                                
                            elif status.startswith("SCAN_END"):
                                self.is_scanning = False
                                self.scan_label.config(text="")
                                if ':' in status:
                                    measures = status.split(':')[1]
                                    self.log_event(f"✓ Scan terminé: {measures} mesures")
                                else:
                                    self.log_event("✓ Scan terminé")
                                    
                            elif status == "READY":
                                self.log_event("✓ Robot prêt")
                            elif status == "STARTED":
                                self.log_event("▶️  Robot démarré")
                            elif status == "STOPPED":
                                self.log_event("⏸️  Robot arrêté")
                    
                    # Messages INFO
                    elif line.startswith("INFO:"):
                        self.log_event(f"📥 {line}")
                        
            except Exception as e:
                if not hasattr(self, '_last_error') or time.time() - self._last_error > 5:
                    self.log_event(f"⚠️  {str(e)[:40]}")
                    print(f"ERREUR: {e}")
                    self._last_error = time.time()
            
            time.sleep(0.02)

    
    def reset_map(self):
        """Réinitialiser la cartographie"""
        self.map_points = []
        self.scan_count = 0
        self.scan_counter.config(text="Scans: 0")
        self.draw_map_grid()
        self.map_info.config(text="Points détectés: 0")
        self.log_event("🗑️  Carte réinitialisée")
    
    def add_map_point(self, angle, distance):
        """Ajouter un point à la carte"""
        self.map_points.append((angle, distance))
        if len(self.map_points) > 2000:  # Limiter à 2000 points
            self.map_points.pop(0)
    
    def draw_radar_grid(self):
        """Dessiner la grille du radar"""
        self.radar_canvas.delete("all")
        
        width = self.radar_canvas.winfo_width()
        height = self.radar_canvas.winfo_height()
        
        if width <= 1 or height <= 1:
            self.root.after(100, self.draw_radar_grid)
            return
        
        center_x = width // 2
        center_y = height // 2
        radius = min(center_x, center_y) - 30
        
        # Cercles concentriques (4 cercles)
        for i in range(1, 5):
            r = radius * i / 4
            self.radar_canvas.create_oval(center_x - r, center_y - r,
                                         center_x + r, center_y + r,
                                         outline='#004400', width=1)
            # Labels de distance
            distance_label = int(self.max_distance * i / 4)
            self.radar_canvas.create_text(center_x + r - 25, center_y - 5,
                                         text=f"{distance_label}cm",
                                         fill='#006600', font=('Courier', 8))
        
        # Lignes radiales (tous les 30°)
        for angle in range(0, 360, 30):
            rad = math.radians(angle)
            x = center_x + radius * math.cos(rad - math.pi/2)
            y = center_y + radius * math.sin(rad - math.pi/2)
            self.radar_canvas.create_line(center_x, center_y, x, y,
                                         fill='#004400', width=1)
            # Labels d'angle
            label_x = center_x + (radius + 15) * math.cos(rad - math.pi/2)
            label_y = center_y + (radius + 15) * math.sin(rad - math.pi/2)
            self.radar_canvas.create_text(label_x, label_y, text=f"{angle}°",
                                         fill='#006600', font=('Courier', 9))
        
        # Point central (robot)
        self.radar_canvas.create_oval(center_x - 5, center_y - 5,
                                     center_x + 5, center_y + 5,
                                     fill='#00ff00', outline='#00ff00')
    
    def draw_map_grid(self):
        """Dessiner la grille de cartographie"""
        self.map_canvas.delete("all")
        
        width = self.map_canvas.winfo_width()
        height = self.map_canvas.winfo_height()
        
        if width <= 1 or height <= 1:
            self.root.after(100, self.draw_map_grid)
            return
        
        center_x = width // 2
        center_y = height // 2
        
        # Grille cartésienne
        grid_spacing = 50
        
        # Lignes verticales
        for x in range(0, width, grid_spacing):
            color = '#004400' if x == center_x else '#002200'
            line_width = 2 if x == center_x else 1
            self.map_canvas.create_line(x, 0, x, height, fill=color, width=line_width)
        
        # Lignes horizontales
        for y in range(0, height, grid_spacing):
            color = '#004400' if y == center_y else '#002200'
            line_width = 2 if y == center_y else 1
            self.map_canvas.create_line(0, y, width, y, fill=color, width=line_width)
        
        # Labels des axes
        self.map_canvas.create_text(center_x + 10, 15, text="Y", 
                                   fill='#00ff00', font=('Courier', 12, 'bold'))
        self.map_canvas.create_text(width - 15, center_y - 10, text="X",
                                   fill='#00ff00', font=('Courier', 12, 'bold'))
        
        # Point central (robot)
        self.map_canvas.create_oval(center_x - 6, center_y - 6,
                                   center_x + 6, center_y + 6,
                                   fill='#0000ff', outline='#0000ff')
        self.map_canvas.create_text(center_x, center_y - 15, text="ROBOT",
                                   fill='#0000ff', font=('Courier', 9, 'bold'))
    
    def animate_radar(self):
        """Animation du radar et mise à jour de la cartographie"""
        # Redessiner la grille radar
        self.draw_radar_grid()
        
        width = self.radar_canvas.winfo_width()
        height = self.radar_canvas.winfo_height()
        
        if width > 1 and height > 1:
            center_x = width // 2
            center_y = height // 2
            radius = min(center_x, center_y) - 30
            
            # ✅ Dessiner les points avec effet de traînée
            for i, (angle, distance, alpha) in enumerate(self.radar_points):
                if 0 < distance < self.max_distance:
                    rad = math.radians(angle - 90)
                    scale = radius / self.max_distance
                    x = center_x + distance * scale * math.cos(rad)
                    y = center_y + distance * scale * math.sin(rad)
                    
                    # Couleur avec fade-out
                    intensity = int(255 * alpha)
                    color = f"#{0:02x}{intensity:02x}{0:02x}"
                    
                    size = 4 if distance < 50 else 3
                    self.radar_canvas.create_oval(x - size, y - size, x + size, y + size,
                                                 fill=color, outline=color)
                
                # Décrémenter alpha (fade-out)
                self.radar_points[i] = (angle, distance, alpha * 0.90)
            
            # Retirer points trop faibles
            self.radar_points = [(a, d, alpha) for a, d, alpha in self.radar_points if alpha > 0.05]
            
            # ✅ Ajouter le point actuel
            if self.current_distance > 0 and self.current_distance < self.max_distance:
                self.radar_points.append((self.current_angle, self.current_distance, 1.0))
            
            # ✅ Dessiner le faisceau de balayage (effet sonar)
            sweep_length = 25 if self.is_scanning else 18
            for i in range(sweep_length):
                angle_offset = i * 2
                alpha = 1.0 - (i / sweep_length)
                intensity = int(255 * alpha * 0.7)
                color = f"#{0:02x}{intensity:02x}{0:02x}"
                
                angle = self.current_angle - angle_offset
                rad = math.radians(angle - 90)
                x = center_x + radius * math.cos(rad)
                y = center_y + radius * math.sin(rad)
                
                line_width = 4 if i < 3 else (3 if i < 8 else 2)
                self.radar_canvas.create_line(center_x, center_y, x, y,
                                             fill=color, width=line_width)
            
            # ✅ Ligne principale du radar (brillante)
            rad = math.radians(self.current_angle - 90)
            x = center_x + radius * math.cos(rad)
            y = center_y + radius * math.sin(rad)
            main_color = '#ff00ff' if self.is_scanning else '#00ff00'
            self.radar_canvas.create_line(center_x, center_y, x, y,
                                         fill=main_color, width=5)
            
            # Cercle lumineux à l'extrémité
            self.radar_canvas.create_oval(x - 5, y - 5, x + 5, y + 5,
                                         fill=main_color, outline='#ffffff', width=2)
            
            # ✅ Afficher distance détectée sur le radar si proche
            if self.current_distance > 0 and self.current_distance < self.max_distance:
                scale = radius / self.max_distance
                obj_x = center_x + self.current_distance * scale * math.cos(rad)
                obj_y = center_y + self.current_distance * scale * math.sin(rad)
                
                # Point obstacle en rouge
                self.radar_canvas.create_oval(obj_x - 6, obj_y - 6, obj_x + 6, obj_y + 6,
                                             fill='#ff0000', outline='#ffffff', width=2)
            
            # Mettre à jour les infos
            dist_text = f"{self.current_distance:.1f}" if self.current_distance > 0 else "---"
            self.radar_info.config(text=f"Angle: {self.current_angle}° | Distance: {dist_text} cm")
        
        # Mettre à jour la cartographie
        self.update_map()
        
        # Répéter l'animation à 60 FPS
        self.root.after(16, self.animate_radar)  # ✅ 16ms = ~60 FPS
    
    def update_map(self):
        """Mettre à jour la vue cartographique"""
        self.draw_map_grid()
        
        width = self.map_canvas.winfo_width()
        height = self.map_canvas.winfo_height()
        
        if width > 1 and height > 1:
            center_x = width // 2
            center_y = height // 2
            scale = min(width, height) / (2 * self.max_distance) * 0.85
            
            # ✅ Dessiner tous les points de la carte
            for angle, distance in self.map_points:
                # Conversion polaire → cartésienne
                rad = math.radians(angle - 90)  # -90 = 0° en haut
                x = center_x + distance * scale * math.cos(rad)
                y = center_y + distance * scale * math.sin(rad)
                
                # Couleur selon distance
                if distance < 50:
                    color = '#ff0000'  # Rouge = proche
                    size = 3
                elif distance < 100:
                    color = '#ffff00'  # Jaune = moyen
                    size = 2
                else:
                    color = '#00ff00'  # Vert = loin
                    size = 2
                
                self.map_canvas.create_oval(x - size, y - size, x + size, y + size,
                                           fill=color, outline=color)
            
            # ✅ Point actuel en cyan (plus gros)
            if 0 < self.current_distance < self.max_distance:
                rad = math.radians(self.current_angle - 90)
                x = center_x + self.current_distance * scale * math.cos(rad)
                y = center_y + self.current_distance * scale * math.sin(rad)
                
                self.map_canvas.create_oval(x - 6, y - 6, x + 6, y + 6,
                                           fill='#00ffff', outline='#ffffff', width=2)
                
                # Afficher distance sur la carte
                self.map_canvas.create_text(x, y - 12, 
                                           text=f"{self.current_distance:.0f}cm",
                                           fill='#ffffff', font=('Courier', 8, 'bold'))
            
            # Mettre à jour le compteur
            self.map_info.config(text=f"Points détectés: {len(self.map_points)}")


if __name__ == "__main__":
    root = tk.Tk()
    app = RadarInterface(root)
    root.mainloop()
