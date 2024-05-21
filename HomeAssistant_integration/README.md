Add to configuration.yaml the following line

> mqtt: !include mqtt.yaml

Copy the file mqtt.yaml in the same folder of configuration.yaml

Change Model, identifiers, uniqueID and name
If replaced in all the blocks, you should have only 1 device in HA

TODO: 
- [ ] add temp and humidity values
- [ ] EXT Battery voltage
- [ ] EXT Battery Relay
- [ ] 220V Relay


To have kWh it should be possible to add an integration like the following...
```yaml
sensor:
  - platform: integration
    source: sensor.XXXX
    name: energy_spent
    unit_prefix: k
    round: 2
```

... but I wasn't able to make it work, BUT was able to add manual integrations for the "power" values

[Docs](https://www.home-assistant.io/integrations/integration/#configuration)



