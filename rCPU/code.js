var cpuDataSets = [new TimeSeries(), new TimeSeries(), new TimeSeries(), new TimeSeries()];

$(function() {
  $("#status").text("");

  get_cpu_use();
  get_temp();
  initChart(0);
  $("#cpu0").after("<br/><canvas id=\"cpu1\" width=\"500\" height=\"100\"></canvas>");
  initChart(1);
  
  setInterval("get_cpu_use()", 500);
  
  setInterval("get_temp()", 5000);
  
});

function get_cpu_use()
{
	$.ajax({
		url: "cpu.api",
		type: "post",
		data: { counter:"0" }
	}).done(function(cpu_info)
	{
       $("#counter").text(cpu_info[0]+" ("+cpu_info.length+" CPUs)");
       cpuDataSets[0].append(new Date().getTime(), cpu_info[0]);
	   cpuDataSets[1].append(new Date().getTime(), cpu_info[1]);
	});
}

function get_temp()
{
    $.ajax({
        url: "temp.api",
        type: "post",
    }).done(function(data)
    {
        $("#temp").text(data);
    });
}

function initChart(cpuId)
{
  var seriesOptions =
  { 
     strokeStyle: 'rgba(0, 255, 0, 1)',
     fillStyle: 'rgba(0, 255, 0, 0.2)',
     lineWidth: 3 
  };
  var timeline = new SmoothieChart(
  {
     labels: {disabled: true},
     maxValue: 100,
     minValue: 0,
     millisPerPixel: 20,
     grid:
     {
        strokeStyle: '#555555',
        lineWidth: 1,
        millisPerLine: 1000,
        verticalSections: 4 
     }
  });
  timeline.addTimeSeries(cpuDataSets[cpuId], seriesOptions);
  timeline.streamTo(document.getElementById('cpu'+cpuId), 500);
}

