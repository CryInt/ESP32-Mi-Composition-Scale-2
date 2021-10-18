import hassapi as hass
import time
from datetime import datetime

import Xiaomi_Scale_Body_Metrics
# https://github.com/lolouk44/xiaomi_mi_scale

#add users here
users = ( 
    {
        "name": "User 1",
        "height": 185,
        "age": "1984-01-01",
        "sex": "male",
        "weight_max" : 120,
        "weight_min" : 70,
    }, 
    {
        "name": "User 2",
        "height": 160,
        "age": "1985-02-02",
        "sex": "female",
        "weight_max" : 90,
        "weight_min" : 50,
    }
)

create_stat_sensors = True

class xiaomiscale(hass.Hass):
	
	def initialize(self):
		self.log("xiaomi scale initialized")
		# Create a sensor in Home Assistant for the MQTT Topic provided by our ESP32 Bridge - we could read this directly via MQTT, but it may be useful to have it as a sensor in HASS
		self.call_service("mqtt/publish", topic='homeassistant/sensor/scale/config', payload='{"name": "scale", "state_topic": "stat/scale", "json_attributes_topic": "stat/scale/attributes"}', retain=True)
		# Listen for this Sensor
		self.listen_state(self.process_scale_data, "sensor.scale", attribute="all")

	def GetAge(self, d1):
		d1 = datetime.strptime(d1, "%Y-%m-%d")
		return abs((datetime.now() - d1).days)/365

	def process_scale_data( self, attribute, entity, new_state, old_state, kwargs ):
		self.log("Process Scale Data")
		weight = float(self.get_state("sensor.scale", attribute="Weight"))
		impedance = float(self.get_state("sensor.scale", attribute="Impedance"))
		units = self.get_state("sensor.scale", attribute="Units")
		timestamp = self.get_state("sensor.scale", attribute="Timestamp")
		mitdatetime = datetime.strptime(timestamp, "%Y-%m-%dT%H:%M:%S.%fZ")
		self._publish( weight, units, mitdatetime, impedance )
		
	def _publish(self, weight, unit, mitdatetime, miimpedance):
		self.log("Scale Publish")
		# Check users for a match
		for user in users :
			if weight > float(user["weight_min"]) and weight < float(user["weight_max"]) :
				self.log(user['name'])
				weightkg = weight
				if unit == 'lbs':
					weightkg = weightkg/2.205
				imp = 0
				if miimpedance:
					imp = int(miimpedance)
				
				lib = Xiaomi_Scale_Body_Metrics.bodyMetrics(weightkg, user["height"], self.GetAge(user["age"]), user["sex"], imp)
				
				disp_weight = 	round( weight, 						2 )
				bmi = 			round( lib.getBMI(), 				2 )
				bmr = 			round( lib.getBMR(), 				2 )
				vfat = 			round( lib.getVisceralFat(),		2 )
				lbm = 			round( lib.getLBMCoefficient(), 	2 )
				bfat = 			round( lib.getFatPercentage(),		2 )
				water = 		round( lib.getWaterPercentage(), 	2 )
				bone = 			round( lib.getBoneMass(),			2 )
				muscle = 		round( lib.getMuscleMass(), 		2 )
				protein = 		round( lib.getProteinPercentage(),	2 )
				
				message = '{'
				message += '"friendly_name":"' + user["name"] + '\'s Body Composition"'
				message += ',"Weight":' + str(disp_weight)
				message += ',"BMI":' + str(bmi)
				message += ',"Basal_Metabolism":' + str(bmr)
				message += ',"Visceral_Fat":' + str(vfat)

				if miimpedance:
					message += ',"Lean_Body_Mass":' + str(lbm)
					message += ',"Body_Fat":' + str(bfat)
					message += ',"Water":' + str(water)
					message += ',"Bone_Mass":' + str(bone)
					message += ',"Muscle_Mass":' + str(muscle)
					message += ',"Protein":' + str(protein)
					
				message += ',"icon":"mdi:run"'
				message += ',"TimeStamp":"' + str(mitdatetime) + '"'
				message += ',"unit_of_measurement":"' + unit + '"' 
				message += '}'
				
				#self.mqtt_publish('stat/bodymetrics/'+user["name"], message, qos=1, retain=True)
				self.set_state("sensor.bodymetrics_" + str(user["name"]).lower(), state = weight, attributes = eval(message) )
				
				
				self.log( "Set sensor.bodymetrics_" + str(user["name"]).lower() + " : " + message )  
				
				if create_stat_sensors is True:
					self.set_state("sensor.bodymetrics_" + str(user["name"]).lower() + "_weight", state = weight, attributes = { "friendly_name" : "Weight", "icon":"mdi:weight-pound", "unit_of_measurement":unit } )
					self.set_state("sensor.bodymetrics_" + str(user["name"]).lower() + "_bmi", state = bmi, attributes = { "friendly_name" : "Body Mass Index", "icon":"mdi:scale-bathroom", "unit_of_measurement":"BMI" } )
					self.set_state("sensor.bodymetrics_" + str(user["name"]).lower() + "_bmr", state = bmr, attributes = { "friendly_name" : "Basal Metabolic Rate", "icon":"mdi:timetable", "unit_of_measurement":"kcal" } )
					self.set_state("sensor.bodymetrics_" + str(user["name"]).lower() + "_vfat", state = vfat, attributes = { "friendly_name" : "Visceral Fat", "icon":"mdi:scale-bathroom", "unit_of_measurement":"%" } )
					if miimpedance:
						self.set_state("sensor.bodymetrics_" + str(user["name"]).lower() + "_lbm", state = lbm, attributes = { "friendly_name" : "Lean Body Mass", "icon":"mdi:scale-bathroom", "unit_of_measurement":"%" } )
						self.set_state("sensor.bodymetrics_" + str(user["name"]).lower() + "_bfat", state = bfat, attributes = { "friendly_name" : "Body Fat", "icon":"mdi:scale-bathroom", "unit_of_measurement":"%" } )
						self.set_state("sensor.bodymetrics_" + str(user["name"]).lower() + "_water", state = water, attributes = { "friendly_name" : "Water", "icon":"mdi:scale-bathroom", "unit_of_measurement":"%" } )
						self.set_state("sensor.bodymetrics_" + str(user["name"]).lower() + "_bone", state = bone, attributes = { "friendly_name" : "Bone Mass", "icon":"mdi:scale-bathroom", "unit_of_measurement":unit } )
						self.set_state("sensor.bodymetrics_" + str(user["name"]).lower() + "_muscle", state = muscle, attributes = { "friendly_name" : "Muscle Mass", "icon":"mdi:scale-bathroom", "unit_of_measurement":unit } )
						self.set_state("sensor.bodymetrics_" + str(user["name"]).lower() + "_protein", state = protein, attributes = { "friendly_name" : "Protein", "icon":"mdi:scale-bathroom", "unit_of_measurement":"%" } )
				break

