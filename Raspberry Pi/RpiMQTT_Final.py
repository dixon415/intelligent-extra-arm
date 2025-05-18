import paho.mqtt.client as mqtt
import subprocess
import time
import json
import re
import requests
import threading

mqtt_broker = "x.x.x.x"
mqtt_port = 1883
mqtt_topic = "classification_topic"
mqtt_alexa_topic = "alexa_topic"
url = "https://script.google.com/macros/s/YOURKEY/exec"

client = mqtt.Client()
client.connect(mqtt_broker, mqtt_port, 60)
payload = {}
headers = {}
last_seen_objects = {}
previous_modified_value = None
process = subprocess.Popen(["edge-impulse-linux-runner", "--continuous"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

def process_payload(payload):
    json_start = payload.find("[")
    if json_start == -1:
        print("Invalid JSON format!")
        return
    json_payload = payload[json_start:]
    try:
        bounding_boxes = json.loads(json_payload)
    except json.JSONDecodeError:
        print("Failed to parse JSON!")
        return ""
    data_to_publish = {"detected_objects": []}
    for box in bounding_boxes:
        data_to_publish["detected_objects"].append({
            "label": box.get("label", "Unknown"),
            "confidence": box.get("value", 0.0),
            "position": {"x": box.get("x", 0), "y": box.get("y", 0)}
        })
    json_output = json.dumps(data_to_publish)
    return json_output

def process_stream_data(json_string):
    global last_seen_objects
    data = json.loads(json_string)
    filtered_objects = {(obj["label"], obj["position"]["x"], obj["position"]["y"]) for obj in data["detected_objects"]}
    if filtered_objects != last_seen_objects:
        last_seen_objects = filtered_objects
        return(json.dumps(data))

def publishClassificationOutput():
	output = process.stdout.readline()
	#print(output)
	if "boundingBoxes" in output:
		cleaned_output = re.split(r"boundingBoxes \d+ms\. ", output)[-1]
		bounding_boxes = process_payload(cleaned_output)
		filtered_bounding_boxes = process_stream_data(bounding_boxes)
		if filtered_bounding_boxes:
			data = json.loads(filtered_bounding_boxes)
			if data:
				for obj in data.get("detected_objects"):
					json_payload = json.dumps(obj)
					client.publish(mqtt_topic, json_payload)
					print(f"Published: {json_payload}")
        
def publishAlexaRequestData():
	global previous_modified_value
	response = requests.request("GET", url, headers=headers, data=payload)
	if response:
		data = json.loads(response.text)
		if data:
			current_modified_value = data["lastModifiedValue"]
			# Check if lastModifiedValue has changed
			if current_modified_value != previous_modified_value:
				inner_dict = json.loads(data["lastColumnValue"])
				component_name = inner_dict["component"]				
				# Update previous_modified_value for future comparisons
				previous_modified_value = current_modified_value
				client.publish(mqtt_alexa_topic, component_name)
				return component_name
			else:
				return ""
		else: 
			return ""
	else:
		return ""
		
	
def classification_loop():
	while True:
		publishClassificationOutput()
	
def alexa_loop():
	while True:
		output = publishAlexaRequestData()
		if output:
			print(output)
		time.sleep(1)

# Run the fetch_data function in a separate thread
classification_thread = threading.Thread(target=classification_loop, daemon=True)
alexa_thread = threading.Thread(target=alexa_loop, daemon=True)
classification_thread.start()
alexa_thread.start()

# Keeping the main thread alive
while True:
    time.sleep(10)
