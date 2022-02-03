#version 330 core
out vec4 FragColor;

struct PointLight {
    vec3 position;

    vec3 specular;
    vec3 diffuse;
    vec3 ambient;

    float constant;
    float linear;
    float quadratic;
};

struct DirLight {
    vec3 direction;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct Material {
    sampler2D texture_diffuse1;
    sampler2D texture_specular1;

    float shininess;
};
in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;

uniform Material material;
uniform PointLight pointLight;
uniform DirLight dirLight;
uniform SpotLight spotLight;

uniform vec3 viewPosition;

vec4 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec4 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec4 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir);

void main()
{
    vec3 normal = normalize(Normal);
    vec3 viewDir = normalize(viewPosition - FragPos);

    vec4 result;
        result += CalcPointLight(pointLight, -normal, FragPos, viewDir);
        result += CalcDirLight(dirLight, -normal, viewDir);
        result += CalcSpotLight(spotLight, -normal, FragPos, viewDir);
    FragColor = result;
}

vec4 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    // spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    // combine results
    vec4 ambient = vec4(light.ambient, 1.0) * texture(material.texture_diffuse1, TexCoords);
    vec4 diffuse = vec4(light.diffuse, 1.0) * diff * texture(material.texture_diffuse1, TexCoords);
    vec4 specular = vec4(light.specular, 1.0) * spec * texture(material.texture_specular1, TexCoords).aaab;
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    return (ambient + diffuse + specular);
}

vec4 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    // combine results
    vec4 ambient = vec4(light.ambient, 1.0) * texture(material.texture_diffuse1, TexCoords);
    vec4 diffuse = vec4(light.diffuse, 1.0) * diff * texture(material.texture_diffuse1, TexCoords);
    vec4 specular = vec4(light.specular, 1.0) * spec * texture(material.texture_specular1, TexCoords).aaab;
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}
vec4 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir)
 {
     vec3 lightDir = normalize(-light.direction);
     // diffuse shading
     float diff = max(dot(normal, lightDir), 0.0);
     // specular shading
     vec3 halfwayDir = normalize(lightDir + viewDir);
     float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
     // combine results
     vec4 ambient = vec4(light.ambient, 1.0) * texture(material.texture_diffuse1, TexCoords);
     vec4 diffuse = vec4(light.diffuse, 1.0) * diff * texture(material.texture_diffuse1, TexCoords);
     vec4 specular = vec4(light.specular, 1.0) * spec * texture(material.texture_specular1, TexCoords).aaab;
     return (ambient + diffuse + specular);
 }