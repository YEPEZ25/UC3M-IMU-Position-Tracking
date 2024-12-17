import socket
import asyncio
import json
import csv
import time
from datetime import datetime
from bleak import BleakClient

# Configuración del servidor socket
HOST = "192.168.125.1" #"127.0.0.1"  # Dirección IP del servidor
PORT = 1025         # Puerto de comunicación

# Dirección MAC del Arduino (obténla al listar dispositivos BLE en tu sistema)
DEVICE_ADDRESS = "4e:4a:46:48:74:cc"

# UUIDs de las características
UUID_JSON = "19B10001-E8F2-537E-4F6C-D104768A1214"
UUID_CONTROL = "19B10002-E8F2-537E-4F6C-D104768A1214"
UUID_CALIBRATION = "19B10003-E8F2-537E-4F6C-D104768A1214"

# Lista para almacenar datos
data_log = []

# Obtener fecha y hora actual
now = datetime.now()
DATE = now.strftime("%Y-%m-%d_%H-%M-%S")  # Formato: Año-Mes-Día_Hora-Minuto-Segundo

async def read_and_send_data(conn, client):
    try:
        while True:
            try:
                # Esperar mensaje del cliente (booleano)
                angle = conn.recv(1024).decode('utf-8').strip()
                print(f"Angulo SetPoint: {angle}")
                if angle.lower() != "finish":
                    # Enviar un booleano para solicitar datos
                    await client.write_gatt_char(UUID_CONTROL, bytearray([1]))

                    # Leer los datos del Arduino
                    data = await client.read_gatt_char(UUID_JSON)
                    json_data = data.decode('utf-8')
                    parsed_data = json.loads(json_data)
                    print(f"X: {parsed_data['X']}, Y: {parsed_data['Y']}, Z: {parsed_data['Z']}")

                    # Guardar datos en la lista
                    data_log.append((parsed_data['X'], parsed_data['Y'], parsed_data['Z'], angle))

                    # Enviar los datos al cliente del socket
                    response = json.dumps(parsed_data)
                    conn.sendall(response.encode('utf-8'))
                else:
                    print("Finishing connection")
                    break
            except (ConnectionResetError, BrokenPipeError):
                print("El cliente se ha desconectado. Finalizando conexión.")
                break
    except Exception as e:
        print(f"Error en BLE o Socket: {e}")               

async def main():
    # Configurar el servidor socket
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.connect((HOST, PORT))
    print(f"Servidor escuchando en {HOST}:{PORT}")

    # Solicitar entrada del eje y radio antes de iniciar la conexión
    eje = input("Ingresar el eje (X, Y o Z): ").strip().upper()
    while eje not in ["X", "Y", "Z"]:
        print("Entrada inválida. Por favor ingresa X, Y o Z.")
        eje = input("Ingresar el eje (X, Y o Z): ").strip().upper()

    radio = input("Ingresar el radio: ").strip()
    while not radio.replace('.', '', 1).isdigit():
        print("Entrada inválida. Por favor ingresa un número válido.")
        radio = input("Ingresar el radio: ").strip()
            
    # Conectar al dispositivo BLE
    try:
        async with BleakClient(DEVICE_ADDRESS, timeout=30.0) as bleclient:
            print("Conectado al dispositivo BLE")
            server_socket.sendall(f"run".encode('utf-8'))
            # Enviar datos iniciales al cliente
            try:
                server_socket.sendall(f"{eje},{radio}".encode('utf-8'))
                print(f"Datos iniciales enviados: Eje={eje}, Radio={radio}")
            except (ConnectionResetError, BrokenPipeError):
                print("El cliente cerró la conexión.")
            # Esperar mensaje de calibración desde el socket
            try:
                print("Esperando el mensaje de calibración desde el socket...")
                calibration = server_socket.recv(1024).decode('utf-8').strip()
                print(f"Mensaje de calibración recibido: {calibration}")
                if calibration.lower() == "calibrate":
                    print("Iniciando calibración...")
                    await bleclient.write_gatt_char(UUID_CALIBRATION, bytearray([1]))

                    # Leer el estado de la calibración
                    data = await bleclient.read_gatt_char(UUID_CALIBRATION)
                    boolean_calibration = data.decode('utf-8')
                    print(f"Calibración: {boolean_calibration}")

                    if boolean_calibration.lower() == "1":
                        print("Calibración finalizada.")
                        server_socket.sendall(f"calibrated".encode('utf-8'))
                        time.sleep(2)
                        
                    else:
                        print("La calibración no se completó correctamente.")
            except (ConnectionResetError, BrokenPipeError):
                print("El cliente cerró la conexión durante la calibración.") 

            # Continuar con la lectura y envío de datos Bluetooth
            await read_and_send_data(server_socket, bleclient)

    except Exception as e:
        print(f"Error en BLE o Socket: {e}")
    finally:
        # Guardar datos en un archivo CSV al cerrar
        with open(f"data_{eje}_{radio}_{DATE}.csv", "w", newline="") as csvfile:
            csvwriter = csv.writer(csvfile, delimiter=';')
            csvwriter.writerow(["X", "Y", "Z", f"SetPoint {eje}"])
            csvwriter.writerows(data_log)
        print(f"Datos guardados en data_{eje}_{radio}_{DATE}.csv")

        server_socket.close()

# Ejecutar el programa principal
asyncio.run(main())
