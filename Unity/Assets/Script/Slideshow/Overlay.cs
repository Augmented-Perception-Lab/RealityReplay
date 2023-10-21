using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class Overlay : MonoBehaviour
{

    public float PlayerFOV;
    private float HalfFOV; 

    public Camera camera;
    public float radius;

    private void OnDrawGizmos()
    {
        float HalfFOV = PlayerFOV / 2;
        Vector3 Point1 = new Vector3(radius * Mathf.Cos(HalfFOV), radius * Mathf.Sin(HalfFOV), 0);
        Vector3 Point2 = new Vector3(radius * Mathf.Cos(-HalfFOV), radius * Mathf.Sin(-HalfFOV), 10);
        Point2 += transform.position;


        Gizmos.color = Color.red;
        Gizmos.DrawSphere(Point1, 1);
        Gizmos.color = Color.yellow;
        Gizmos.DrawSphere(Point2, 1);
        Vector3 Point3 = (Point1 + Point2) / 2;
        Gizmos.color = Color.green;
        Gizmos.DrawSphere(Point3, 1);
        this.gameObject.transform.position = Point3;
        float scale = Mathf.Abs(Point1.x - Point2.x) / 9;

        this.gameObject.transform.localScale = new Vector3(scale, scale, scale);


    }
    // Start is called before the first frame update
    void Start()
    {
        PlayerFOV = camera.fieldOfView;
        float HalfFOV = PlayerFOV / 2;

        Vector3 Point1 = new Vector3(0, radius * Mathf.Sin(HalfFOV), radius * Mathf.Cos(HalfFOV));
        Vector3 Point2 = new Vector3(0, radius * Mathf.Sin(-HalfFOV), radius * Mathf.Cos(-HalfFOV));
        // offset this by object transform? 
        Point2 += transform.position;


        float scale = Mathf.Abs(Point1.x - Point2.x);
        this.gameObject.transform.position = (Point1 + Point2) / 2;
        this.gameObject.transform.localScale = new Vector3(scale, scale, scale);
    }

    // Update is called once per frame
    void Update()
    {
        
    }
}
